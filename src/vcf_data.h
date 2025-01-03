/*
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#ifndef __VCF_DATA_H__
#define __VCF_DATA_H__

#include <stdint.h>
#include <map>
#include <vector> 
#include <string>
#include "util.h"
#include "htslib/vcf.h"

enum genotype {UNPHASED, ALLELE1, ALLELE2, NONSPECIFIC};

const char* gt_str(genotype gt);
bool gt_conflict(genotype g1, genotype g2);   		// true if (g1,g2) == (ALE1, ALE2) or (ALE2, ALE1)
bool gt_explicit_same(genotype g1, genotype g2);    // true if g1 == g2 == ALE1 or g1 == g2 == ALE2
bool gt_implicit_same(genotype g1, genotype g2);    // true if explicit_same or (UNPHASE, NONSPECIFIC) same. (UNPHASE, ALE1) is false
bool gt_as(genotype g); 							// true if ALLELE1, ALLELE2

class vcf_data
{
public:  						
	vcf_data();								
	vcf_data(std::string);

public:
	map < string, map <int, map<string, genotype> > > vcf_pos_map; // map <string chrm, map<int pos, map<string var, genotype> > >   map of variant posisions and vector_of_variant_sequences
	map < string, map <int, int > > vcf_ale_len;		// map <string chrm, map<int pos, int length > >		    map of variant positions and lengths_on_reference

private:
	int read_as_counts(const std::string &); // read .asf file, make vcf_map and vcf_pos_map

public:
	genotype get_genotype(string chrm, int pos, string ale); // return UNPHASED if not found
	static string graphviz_gt_color_shape(genotype gt, int vertex_type);
	static int increse_it(map <int, map <string, genotype> >::iterator &it1, map <int, int >::iterator &it2);
	int print();

};


#endif