/*
 * tb2dgvns.cpp
 *
 *  Created on: 15 December 2016
 *      Author: Abdelkader Ouali
 *      PhD Student: LITIO, University of Oran ; GREYC, University of Caen.
 */

#include "tb2dgvns.hpp"
#include "core/tb2wcsp.hpp"
#ifdef BOOST

bool VNSSolver::solve()
{
    // Initialization
    beginSolve(MAX_COST);
    try {
        lastUb = MAX_COST;
        lastSolution.clear();
        preprocessing(MAX_COST);
    } catch (Contradiction) {
        wcsp->whenContradiction();
        if (lastUb < MAX_COST)
            wcsp->setSolution(lastUb, &lastSolution);
        endSolve(lastUb < MAX_COST, lastUb, true);
        return (lastUb < MAX_COST);
    }

    // Compute the Initial solution
    bool complete = false;
    bestSolution.clear();
    bestUb = generateInitSolution(ToulBar2::vnsInitSol, bestSolution, complete);
    if (ToulBar2::verbose >= 1)
        cout << "VNS: initial solution with" << ((complete) ? " optimal" : "") << " cost " << std::fixed << std::setprecision(ToulBar2::decimalPoint) << wcsp->Cost2ADCost(bestUb) << std::setprecision(DECIMAL_POINT) << endl;
    try {
        wcsp->updateUb(bestUb);
        wcsp->enforceUb();
        wcsp->propagate(); // warning! some variables might become assigned by propagation to a value different than in bestSolution
        if (unassignedVars->getSize() == 0) {
            lastUb = MAX_COST;
            lastSolution.clear();
            ToulBar2::lds = 0;
            newSolution();
            if (lastUb < MAX_COST)
                wcsp->setSolution(lastUb, &lastSolution);
            endSolve(lastUb < MAX_COST, lastUb, true);
            return (lastUb < MAX_COST);
        }
    } catch (Contradiction) {
        wcsp->whenContradiction();
        if (bestUb < MAX_COST)
            wcsp->setSolution(bestUb, &bestSolution);
        endSolve(bestUb < MAX_COST, bestUb, true);
        return (bestUb < MAX_COST);
    }

    assert((int)wcsp->numberOfUnassignedVariables() == unassignedVars->getSize());
    ToulBar2::vnsLDSmax = min(ToulBar2::vnsLDSmax, (int)wcsp->getDomainSizeSum() - (int)wcsp->numberOfUnassignedVariables());
    ToulBar2::vnsLDSmin = min(ToulBar2::vnsLDSmin, ToulBar2::vnsLDSmax);
    ToulBar2::vnsKmax = min(ToulBar2::vnsKmax, (int)wcsp->numberOfUnassignedVariables());
    ToulBar2::vnsKmin = min(ToulBar2::vnsKmin, ToulBar2::vnsKmax);
    assert(ToulBar2::vnsLDSmin >= 0);
    assert(ToulBar2::vnsLDSmax >= 0);
    assert(ToulBar2::vnsLDSmin <= ToulBar2::vnsLDSmax);
    assert(ToulBar2::vnsKmin >= 0);
    assert(ToulBar2::vnsKmax >= 0);
    assert(ToulBar2::vnsKmin <= ToulBar2::vnsKmax);

    // cluster tree initialized AFTER generating initial solution
    NeighborhoodStructure* h = NULL;
    switch (ToulBar2::vnsNeighborVarHeur) {
    case RANDOMVAR:
        if (ToulBar2::verbose >= 1)
            cout << "Random Variables Neighborhood Structure selection" << endl;
        h = new RandomNeighborhoodChoice();
        break;
    case CLUSTERRAND:
        if (ToulBar2::verbose >= 1)
            cout << "Random Clusters Neighborhood Structure selection" << endl;
        h = new RandomClusterChoice();
        break;
    default:
        cerr << "Unknown Neighborhood Structure" << endl;
        exit(EXIT_FAILURE);
    }
    h->init(wcsp, this);
    if (ToulBar2::verbose >= 0 && ToulBar2::vnsNeighborVarHeur == CLUSTERRAND && ((ClustersNeighborhoodStructure*)h)->getSize() > 1) {
        ClustersNeighborhoodStructure* ch = (ClustersNeighborhoodStructure*)h;
        if (ToulBar2::verbose >= 1 || ToulBar2::debug)
            ch->printClusters(cout);
        cout << "Problem decomposition in " << ch->getSize() << " clusters with size distribution: min: " << ch->getMinClusterSize() << " median: " << ch->getMedianClusterSize() << " mean: " << ch->getMeanClusterSize() << " max: " << ch->getMaxClusterSize() << endl;
    }
    //vns/lds+cp
    bool stop = false;
    Long nbRestart = 1;
    Long restart = 1;
    int lds = ToulBar2::vnsLDSmin;
    while (!stop && !complete && bestUb > ToulBar2::vnsOptimum) {
        if (ToulBar2::verbose >= 0 && ToulBar2::restart > 1 && ToulBar2::lds)
            cout << "****** Restart " << nbRestart << " with " << lds << " discrepancies and UB=" << std::fixed << std::setprecision(ToulBar2::decimalPoint) << wcsp->Cost2ADCost(bestUb) << std::setprecision(DECIMAL_POINT) << " ****** (" << nbNodes << " nodes)" << endl;
        Long rank = 1;
        int k = ToulBar2::vnsKmin;
        while (!complete && k <= ToulBar2::vnsKmax && bestUb > ToulBar2::vnsOptimum) {
            //neighborhood and partial instantiation
            set<int> neighborhood = h->getNeighborhood(k);
            if (ToulBar2::verbose >= 1) {
                cout << "LDS " << lds << " Neighborhood " << k << ": ";
                for (set<int>::iterator it = neighborhood.begin(); it != neighborhood.end(); it++)
                    cout << " " << *it;
                cout << endl;
            }
            vector<int> variables;
            variables.reserve(unassignedVars->getSize());
            vector<int> values;
            values.reserve(unassignedVars->getSize());
            for (BTList<Value>::iterator iter = unassignedVars->begin(); iter != unassignedVars->end(); ++iter) {
                int v = *iter;
                if (neighborhood.find(v) == neighborhood.end()) {
                    variables.push_back(v);
                    values.push_back(bestSolution[v]);
                }
            }

            //repair
            ToulBar2::vnsKcur = k;
            ToulBar2::vnsLDScur = (ToulBar2::lds) ? lds : -1;
            if (ToulBar2::lds)
                complete = repair_recursiveSolve(lds, variables, values, bestUb);
            else
                complete = repair_recursiveSolve(variables, values, bestUb);

            //updating
            if (lastUb >= bestUb) {
                if (h->incrementK()) {
                    rank++;
                    if (k < ToulBar2::vnsKmax) {
                        switch (ToulBar2::vnsKinc) {
                        case VNS_ADD1:
                            k++;
                            break;
                        case VNS_MULT2:
                            k *= 2;
                            break;
                        case VNS_LUBY:
                            k = ToulBar2::vnsKmin * luby(rank);
                            break;
                        case VNS_ADD1JUMP:
                            if (ToulBar2::vnsNeighborVarHeur == RANDOMVAR || k < ((ClustersNeighborhoodStructure*)h)->getMaxClusterSize() + ((ClustersNeighborhoodStructure*)h)->getSize() - 1)
                                k++;
                            else
                                k = ToulBar2::vnsKmax;
                            break;
                        default:
                            cerr << "Unknown neighborhood size increment strategy inside VNS (see option -kinc)!" << endl;
                            exit(EXIT_FAILURE);
                        }
                        k = min(k, ToulBar2::vnsKmax);
                    } else
                        k++;
                    //                    cout << "rank: " << rank << " luby: " << luby(rank) << " k: " << k << " lds: " << ToulBar2::lds << endl;
                }
            } else {
                rank = 1;
                k = ToulBar2::vnsKmin;
                restart = 1;
                lds = ToulBar2::vnsLDSmin;
                bestUb = lastUb;
                for (int v = 0; v < (int)wcsp->numberOfVariables(); v++) {
                    assert(lastSolution.find(v) != lastSolution.end());
                    bestSolution[v] = lastSolution[v];
                }
                if (ToulBar2::verbose >= 1)
                    cout << "VNS: new solution with cost " << std::fixed << std::setprecision(ToulBar2::decimalPoint) << wcsp->Cost2ADCost(bestUb) << std::setprecision(DECIMAL_POINT) << endl;
            }
        }
        if (!complete && bestUb > ToulBar2::vnsOptimum) {
            nbRestart++;
            restart++;
            if (nbRestart <= ToulBar2::restart && lds < ToulBar2::vnsLDSmax) {
                if (ToulBar2::lds) {
                    switch (ToulBar2::vnsLDSinc) {
                    case VNS_ADD1:
                        lds++;
                        break;
                    case VNS_MULT2:
                        lds *= 2;
                        break;
                    case VNS_LUBY:
                        lds = ToulBar2::vnsLDSmin * luby(restart);
                        break;
                    default:
                        cerr << "Unknown LDS increment strategy inside VNS (see option -linc)!" << endl;
                        exit(EXIT_FAILURE);
                    }
                    lds = min(lds, ToulBar2::vnsLDSmax);
                }
            } else
                stop = true;
        }
    }

    if (ToulBar2::vnsOutput)
        ToulBar2::vnsOutput.close();

    if (bestUb < MAX_COST)
        wcsp->setSolution(bestUb, &bestSolution);
    endSolve(bestUb < MAX_COST, bestUb, complete);

    return (bestUb < MAX_COST);
}

#endif

/* Local Variables: */
/* c-basic-offset: 4 */
/* tab-width: 4 */
/* indent-tabs-mode: nil */
/* c-default-style: "k&r" */
/* End: */
