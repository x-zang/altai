/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#ifndef __GTF_ITEM_H__
#define __GTF_ITEM_H__

#include <string>
#include <stdint.h>
#include "src/as_pos32.hpp"

using namespace std;

class item
{
public:
	item(const string &s);

public:
	int parse(const string &s);
	bool operator<(const item &ge) const;
	int print() const;
	int length() const;

public:
	string seqname;
	string source;
	string feature;
	string gene_id;
	string transcript_id;
	string transcript_type;
	string gene_type;
	as_pos32 start;
	as_pos32 end;
	double score;
	char strand;
	int frame;
	double coverage;
	double FPKM;
	double RPKM;
	double TPM;
};

#endif
