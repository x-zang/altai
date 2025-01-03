/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#ifndef __PARTIAL_EXON_H__
#define __PARTIAL_EXON_H__

#include <stdint.h>
#include <vector>
#include <string>
#include "as_pos32.hpp"
#include "vcf_data.h"

using namespace std;

class partial_exon
{
public:
	partial_exon(as_pos32 _lpos, as_pos32 _rpos, int _ltype, int _rtype, genotype _gt);

public:
	as_pos32 lpos;					// the leftmost boundary on reference
	as_pos32 rpos;					// the rightmost boundary on reference
	int ltype;						// type of the left boundary
	int rtype;						// type of the right boundary
	genotype gt;

	int rid;						// parental region id
	int rid2;						// parental region's pexon index 
	int pid;						// index in the bundle pexons
	int type;						// label 0: normal, -9: EMPTY_VERTEX, -1: pseudo AS pexon
	double ave;						// average abundance
	double max;						// maximum abundance
	double dev;						// standard-deviation of abundance

public:	
	bool operator < (partial_exon pe);
	bool operator < (const partial_exon pe) const;

public:
	// string label() const;
	bool is_allelic() const;				// containing AS positions or not
	int assign_as_cov(double ave, double max, double dev);
	int print(int index) const;
};

#endif
