/*
Part of Altai
(c) 2022 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#ifndef __PHASER_H__
#define __PHASER_H__

#include <vector>
#include <map>
#include "splice_graph.h"
#include "hyper_set.h"
#include "as_pos32.hpp"
#include "bundle.h"
#include "scallop.h"
#define MEPD map<edge_descriptor, pair<double, double> >

class phaser
{
public:
	phaser(scallop& sc, splice_graph* gr1, hyper_set* hs1, splice_graph* gr2, hyper_set* hs2);
	phaser(scallop& sc, splice_graph* gr1, hyper_set* hs1, splice_graph* gr2, hyper_set* hs2, scallop* sc1, scallop* sc2);

private:
    const scallop& sc;
    splice_graph& gr;
    MED ewrt1;              // edge weight in allele1
    MED ewrt2;              // edge weight in allele2
    vector<double> vwrt1;   // vertex weight in allele1
    vector<double> vwrt2;   // vertex weight in allele2

    double vwrtbg1;        // sum bg weights of allele 1
    double vwrtbg2;        // sum bg weights of allele 2
    double ewrtbg1;        // normalized bg ratio of allele 1
    double ewrtbg2;        // normalized bg ratio of allele 2
    double ewrtratiobg1;   // normalized bg ratio of allele 1
    double ewrtratiobg2;   // normalized bg ratio of allele 2

    MEE x2y_1;             // use x2y to map original edge to new edge, in allele 1
	MEE y2x_1;             // use y2x to map new edge to original edge
    MEE x2y_2;             // use x2y to map original edge to new edge, in allele 2
	MEE y2x_2;

    splice_graph* pgr1;    // pointer to sg of allele1, value to return
    splice_graph* pgr2;    // pointer to sg of allele2, value to return
    hyper_set* phs1;       // pointer to hs of allele2, value to return
    hyper_set* phs2;       // pointer to hs of allele2, value to return
    scallop* sc1;          // pointer to sc of allele1, value to return
    scallop* sc2;          // pointer to sc of allele2, value to return

private:
    string strategy;                    
    double epsilon = 0.01;  
                            
private:
    int init();
    int assign_gt();
    int split_gr();
    int refine_allelic_graphs();
    int split_hs();
    int allelic_transform(splice_graph* pgr, hyper_set* phs, MEE& x2y);

private:
    pair<double, double> get_as_ratio(int i);
    vector<int> sort_nodes_by_currecnt_mae(const set<int>& s);
    bool split_local(int i);
    bool split_global(int i);
    bool split_by_ratio(int v, const PEEI& in, const PEEI& out, double r1);                  // split edges of vertex v, by ratio 
    int split_by_phasing(int v, const PEEI& in, const PEEI& out, double r1);                // split edges of vertex v, by phasing path
    int split_by_min_parsimony(int v, const PEEI& in, const PEEI& out, double r1);          // split edges of vertex v, by parsimony
    pair<double, double> normalize_epsilon(double x, double y);                             // adjustment to allele ratio. new_r1 = (r1+eps) / (r1+r2+2*eps). returns<-1, -1> if both input are 0

};

#endif
