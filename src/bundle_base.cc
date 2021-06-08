/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#include <cassert>
#include <cstdio>
#include <cmath>
#include <climits>

#include "bundle_base.h"
#include "as_pos32.hpp"

bundle_base::bundle_base()
{
	tid = -1;
	chrm = "";
	lpos = 1 << 30;
	rpos = 0;
	strand = '.';
}

bundle_base::~bundle_base()
{}

int bundle_base::add_hit(const hit &ht)
{
	// store new hit
	hits.push_back(ht);

	// calcuate the boundaries on reference
	if(ht.pos < lpos) lpos = ht.pos;
	if(ht.rpos > rpos) rpos = ht.rpos;

	// set tid
	if(tid == -1) tid = ht.tid;
	assert(tid == ht.tid);

	// set strand
	if(hits.size() <= 1) strand = ht.strand;
	assert(strand == ht.strand);

	// set bundle is_allelic
	if (ht.apos.size() != 0) is_allelic = true;

	// DEBUG
	/*
	if(strand != ht.strand)
	{
		printf("strand = %c, ht.strand = %c, ht.xs = %c,\n", strand, ht.strand, ht.xs);
	}
	*/

	for(int k = 0; k < ht.itvm.size(); k++)
	{
		as_pos32 s = high32(ht.itvm[k]);
		as_pos32 t = low32(ht.itvm[k]);
		assert(s.ale == "$");
		assert(t.ale == "$");
		mmap += make_pair(ROI(s, t), 1);
	}

	for(int k = 0; k < ht.itvi.size(); k++)
	{
		as_pos32 s = high32(ht.itvi[k]);
		as_pos32 t = low32(ht.itvi[k]);
		assert(s.ale == "$");
		assert(t.ale == "$");
		imap += make_pair(ROI(s, t), 1);
	}

	for(int k = 0; k < ht.itvd.size(); k++)
	{
		as_pos32 s = high32(ht.itvd[k]);
		as_pos32 t = low32(ht.itvd[k]);
		assert(s.ale == "$");
		assert(t.ale == "$");
		imap += make_pair(ROI(s, t), 1);
	}

	if (vcf_file == "")
		nammap = mmap;
	else
	{
		for(int k = 0; k < ht.itvna.size(); k++)
		{
			as_pos32 s = high32(ht.itvna[k]);
			as_pos32 t = low32(ht.itvna[k]);
			assert(s.ale == "$" && t.ale == "$");
			nammap += make_pair(ROI(s, t), 1);
		}
	}

	return 0;
}

bool bundle_base::overlap(const hit &ht) const
{
	if(mmap.find(ROI(ht.pos, ht.pos + 1)) != mmap.end()) return true;
	if(mmap.find(ROI(ht.rpos - 1, ht.rpos)) != mmap.end()) return true;
	return false;
}

int bundle_base::clear()
{
	tid = -1;
	chrm = "";
	lpos = 1 << 30;
	rpos = 0;
	strand = '.';
	hits.clear();
	mmap.clear();
	imap.clear();
	nammap.clear();
	return 0;
}
