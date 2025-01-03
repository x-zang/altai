/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#ifndef __GTF_GENE_H__
#define __GTF_GENE_H__

#include <fstream>
#include <vector>
#include <set>
#include <map>

#include "item.h"
#include "transcript.h"
#include "src/as_pos32.hpp"

using namespace std;

class gene
{
public:
	vector<transcript> transcripts;			
	map<string, int> t2i;

public:
	// build
	int add_transcript(const transcript &t);
	int add_transcript(const item &e);
	int add_exon(const item &e);

	// modify
	int assign(const vector<transcript> &v);
	int sort();
	int shrink();
	int clear();
	// int set_gene_id(const string &id);
	int assign_RPKM(double factor);

	// filter
	int filter_single_exon_transcripts();
	int filter_low_coverage_transcripts(double min_coverage);

	// set<as_pos32> get_exon_boundaries() const;
	PI32 get_bounds() const;
	// string get_seqname() const;
	// string get_gene_id() const;
	// char get_strand() const;

	// write
	int write(ofstream &fout) const;	
};

#endif
