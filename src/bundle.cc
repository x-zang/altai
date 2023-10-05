/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Scallop2
(c) 2021 by  Qimin Zhang, Mingfu Shao, and The Pennsylvania State University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>  

#include "bundle.h"
#include "bundle_bridge.h"
#include "region.h"
#include "config.h"
#include "util.h"
#include "undirected_graph.h"
#include "as_pos.hpp"
#include "as_pos32.hpp"
#include "interval_map.h"

using namespace std;

bundle::bundle(bundle_base &b)
	: bb(b), br(b)
{
	br.build();
	prepare();
}

bundle::~bundle()
{}


int bundle::prepare()
{
	compute_strand();
	build_intervals();
	build_partial_exons();

	pexon_jset(jset);
	return 0;
}

int bundle::build(int mode, bool revise)
{
	build_splice_graph(mode);

	// if(revise == true) revise_splice_graph(); // FIXME: this might result an error for allelic regions, default true
	refine_splice_graph();
	build_hyper_set();
	return 0;
}

int bundle::compute_strand()
{
	if(library_type != UNSTRANDED) assert(bb.strand != '.');
	if(library_type != UNSTRANDED) return 0;

	int n0 = 0, np = 0, nq = 0;
	for(int i = 0; i < bb.hits.size(); i++)
	{
		if(bb.hits[i].xs == '.') n0++;
		if(bb.hits[i].xs == '+') np++;
		if(bb.hits[i].xs == '-') nq++;
	}

	if(np > nq) bb.strand = '+';
	else if(np < nq) bb.strand = '-';
	else bb.strand = '.';

	return 0;
}

int bundle::build_intervals()
{
	fmap.clear();
	set<hit*> added_hit;
	for(int i = 0; i < br.fragments.size(); i++)
	{
		fragment &fr = br.fragments[i];
		if(fr.paths.size() != 1 || fr.paths[0].type != 1) continue;
		const vector<as_pos32>& vv = br.get_aligned_intervals(fr);
		if(vv.size() <= 0) continue;
		assert(vv.size() % 2 == 0);

		/*
		if (DEBUG_MODE_ON && verbose >= 10)
		{
			for(auto h: {fr.h1, fr.h2})
			{
				hit ht = *h;
				cout << ht.qname << "itv size bridged=" << ht.itv_align.size() << " =" ;
				ht.print(true);
			}
		}
		*/

		for(int k = 0; k < vv.size() / 2; k++)
		{
			int32_t p = vv[2 * k + 0];
			int32_t q = vv[2 * k + 1];
			fmap += make_pair(ROI(p, q), 1);
			// if (DEBUG_MODE_ON && verbose >= 10) cout <<"itv added" << p << "-" << q << endl;
		}
		added_hit.insert(fr.h1);
		added_hit.insert(fr.h2);
	}

	for(int i = 0; i < bb.hits.size(); i++)
	{
		hit &ht = bb.hits[i];
		if((ht.flag & 0x100) >= 1 && !use_second_alignment) continue;
		if(added_hit.find(&ht) != added_hit.end()) continue;
		// if(ht.bridged == true) continue;
		// if(br.breads.find(ht.qname) != br.breads.end()) continue;

		for(int k = 0; k < ht.itv_align.size(); k++)
		{
			int32_t s = high32(ht.itv_align[k]);
			int32_t t = low32(ht.itv_align[k]);
			fmap += make_pair(ROI(s, t), 1);
		}
		// cout << ht.qname << "unbridged itv size=" << ht.itv_align.size() << endl;
		// ht.print();
	}
	return 0;
}

int bundle::build_partial_exons()
{
	pexons.clear();
	regional.clear();

	set<int32_t> m1, m2; // junction site
	for (auto&& j: br.junctions)
	{
		m1.insert(j.lpos.p32);
		m2.insert(j.rpos.p32);
	}

	vector<region>& regions = br.regions;
	// add non-AS pexons
	for (int i = 0; i < regions.size(); i++)
	{
		region& r =  regions[i];
		if(!r.is_allelic()) 
		{
			r.rebuild(&fmap); 
			for(int k = 0; k < r.pexons.size(); k++)
			{
				partial_exon& rpe = r.pexons[k];
				partial_exon pe (rpe);
				rpe.rid = i;
				rpe.rid2 = k;
				pe.rid = i;
				pe.rid2 = k;
				pexons.push_back(pe);
			}
		}
	}

	// add AS pexons directly
	for (int i = 0; i < regions.size(); i++)
	{
		region& r =  regions[i];
		if(r.is_allelic()) 
		{
			assert(r.pexons.size() == 0);
			int ltype = -1, rtype = -1;

			// left side is not junction, not var, & (empty vertex or no pexon)  => ltype += START_BOUNDARY;
			if (m1.find(r.lpos.p32) != m1.end()) ltype = r.ltype;
			else if (i >= 1 && regions[i-1].is_allelic()) ltype = r.ltype;
			else if (i >= 1 && regions[i-1].pexons.size() == 0) ltype = START_BOUNDARY;
			else if (i >= 1 && regions[i-1].pexons[regions[i-1].pexons.size() - 1].type != EMPTY_VERTEX) ltype = r.ltype;
			else ltype = START_BOUNDARY;

			// right side is not junction, not var, & empty => rtype += END_BOUNDARY;
			if (m2.find(r.rpos.p32) != m2.end()) rtype = r.rtype;
			else if (i < regions.size() - 1 && regions[i+1].is_allelic()) rtype = r.rtype;
			else if (i < regions.size() - 1 && regions[i+1].pexons.size() == 0) rtype = END_BOUNDARY;
			else if (i < regions.size() - 1 && regions[i+1].pexons[0].type != EMPTY_VERTEX) rtype = r.rtype;
			else rtype = END_BOUNDARY;

			assert(ltype != -1);
			assert(rtype != -1);
			assert(r.ave != 0);
			// cout << "as pexon" << r.lpos.aspos32string() << "-" << r.rpos.aspos32string() <<endl;
			// assert(r.gt != UNPHASED);  // assuming all var phased //FIXME: not always true, how to handle potential errors

			partial_exon pe(r.lpos, r.rpos, ltype, rtype, r.gt);
			pe.assign_as_cov(r.ave, r.max, r.dev);
			pe.rid = i;
			pe.rid2 = 0;
			pe.type = 0;  // assert not EMPTY_VERTEX
			r.pexons.push_back(pe);
			assert(r.pexons.size() == 1);
			pexons.push_back(pe);
		}
	}

	// sort, make pe.pid & regional
	sort(pexons.begin(), pexons.end());
	for (int i = 0; i < pexons.size(); i ++)
	{
		partial_exon& pe = pexons[i];
		pe.pid = i;
		if((pe.lpos != bb.lpos || pe.rpos != bb.rpos) && (pe.ltype & START_BOUNDARY) && (pe.rtype & END_BOUNDARY)) 
			regional.push_back(true);
		else regional.push_back(false);		
		// region.pexons and bundle.pexons should have the same rid, rid2, pid
		assert(pe.rid >= 0 && pe.rid < regions.size());
		assert(pe.rid2 >= 0 && pe.rid2 < regions[pe.rid].pexons.size());
		partial_exon& rpe = regions[pe.rid].pexons[pe.rid2];
		assert(pe.lpos == rpe.lpos);
		assert(pe.rpos == rpe.rpos);
		assert(rpe.pid == -1);
		assert(rpe.rid == pe.rid);
		assert(rpe.rid2 == pe.rid2);
		if(i >= 1) assert(pe.lpos.p32 >= pexons[i-1].lpos);
		rpe.pid = i;
	}

	if(DEBUG_MODE_ON)
	{
		for(const region& r: regions)
			for(const partial_exon& pe: r.pexons)
				assert(pe.pid >= 0 && pe.pid <= pexons.size());
	}

	/*
	if (DEBUG_MODE_ON)
	{
		assert(regional.size() == pexons.size());
		// assert(pexons.size() >= regions.size()); // not true
		printf("size of regions %d, size of pexon %d \n", regions.size(), pexons.size());
		if (verbose >= 3 ) cout << "print pexons:\n";

		for (int i = 0; i< pexons.size(); i++) 
		{
			if(verbose >= 3) 
			{
				cout <<"regional?" <<regional[i] << "\t" ; 
				pexons[i].print(i); 
			}
			if (i < pexons.size() - 1) 
			{
				bool b0 =  pexons[i].is_allelic() && pexons[i+1].is_allelic();
				bool b1 =  pexons[i].rpos.leftsameto(pexons[i+1].lpos);
				bool b2 =  b0 && pexons[i].lpos.samepos(pexons[i+1].lpos) && pexons[i].rpos.samepos(pexons[i+1].rpos);
				assert(b1 || b2);

				// note: regions are also sorted
				partial_exon& p1 = pexons[i];
				partial_exon& p2 = pexons[i + 1];
				cout << p1.rid << " " << p2.rid << ": " << p1.pid << " " << p2.pid << endl;
				assert(p2.rid > p1.rid || (p2.rid == p1.rid && p2.pid == p1.pid + 1));
			}
		}			
	}
	*/

	return 0;
}

/*
**	equivalent to `junctions` and `link_partial_exons`
**	jset_to_fill := map < (in-idx, out-idx) , count>
**	computed from fragments' paths/ hits' vlist, which are indices of regions (i.e. jset in bridger)
**	convert this region index jset to pexon index jset
 */
int bundle::pexon_jset(map<pair<int, int>, pair<int,char> >& pexon_jset)
{	
	const vector<region>& regions = br.regions;
	pexon_jset.clear(); 						// to be filled

	// bridged fragments
	map<pair<int, int>, vector<hit*>> m;
	for(int i = 0; i < br.fragments.size(); i++)
	{
		fragment &fr = br.fragments[i];
		if(fr.paths.size() != 1 || fr.paths[0].type != 1) continue;
		const vector<int32_t>& vv = br.get_splices_region_index(fr);
		if(vv.size() <= 0) continue;

		for(int k = 0; k < vv.size() - 1; k++)
		{
			pair<int, int> xy {vv[k], vv[k+1]};
			if(m.find(xy) == m.end())
			{
				vector<hit*> hv;
				hv.push_back(fr.h1);
				m.insert({xy, hv});
			}
			else
			{
				m[xy].push_back(fr.h1);
			}
		}
	}

	// unbridged hits
	for(int i = 0; i < bb.hits.size(); i++)
	{
		if(bb.hits[i].bridged == true) continue;
		if((bb.hits[i].flag & 0x100) >= 1) continue;
		if(br.breads.find(bb.hits[i].qname) != br.breads.end()) continue;

		vector<int> v = decode_vlist(bb.hits[i].vlist);
		if(v.size() == 0) continue;

		for(int k = 0; k < v.size() - 1; k++)
		{
			pair<int, int> xy {v[k], v[k+1]};
			if(m.find(xy) == m.end())
			{
				vector<hit*> hv;
				hv.push_back(&(bb.hits[i]));
				m.insert({xy, hv});
			}
			else
			{
				m[xy].push_back(&(bb.hits[i]));
			}
		}
	}

	// compute strandness & region index to pexon index
	// map<pair<int, int>, vector<hit*>> m;
	if (DEBUG_MODE_ON)
	{
		for(auto it = m.begin(); it != m.end(); it++)
		{
			int rid1 = it->first.first;
			int rid2 = it->first.second;
			int c = it->second.size();
			cout << "jset m: " << rid1 << "--" << rid2 << ", counts = " << c << endl;
		}
	}

	map<pair<as_pos32, as_pos32>, int> pmap;
	for(int i = 0; i < pexons.size(); i++)
	{
		const partial_exon& pe = pexons[i];
		assert(pmap.find({pe.lpos, pe.rpos}) == pmap.end());
		pmap[{pe.lpos, pe.rpos}] = i;
	}

	map<pair<int, int>, vector<hit*>>::iterator it;
	for(it = m.begin(); it != m.end(); it++)
	{
		vector<hit*> &v = it->second;
		if(v.size() < min_splice_boundary_hits) continue;

		int rid1 = it->first.first;
		int rid2 = it->first.second;
		assert(rid1 >= 0 && rid1 < regions.size());
		assert(rid2 >= 0 && rid2 < regions.size());
		assert(rid1 < rid2);

		int pid1 = -1;
		int pid2 = -1;
		const vector<partial_exon>& pexons1 = regions[rid1].pexons;
		const vector<partial_exon>& pexons2 = regions[rid2].pexons;
		
		// rid to pid
		// assuming an edge always connect region1's last pexon to region2's first exon
		if (pexons1.size() >= 1 && pexons2.size() >= 1)
		{				
			const partial_exon& pe1 = pexons1[pexons1.size() - 1];
			const partial_exon& pe2 = pexons2[0];
			assert(pmap.find({pe1.lpos, pe1.rpos}) != pmap.end());
			assert(pmap.find({pe2.lpos, pe2.rpos}) != pmap.end());
			pid1 = pmap[{pe1.lpos, pe1.rpos}];
			pid2 = pmap[{pe2.lpos, pe2.rpos}];

			assert(pid1 < pid2);
			if(!pexons[pid1].rpos.samepos(regions[rid1].rpos)) pid1 = -1;
			if(!pexons[pid2].lpos.samepos(regions[rid2].lpos)) pid2 = -1;
		}
		if (pid1 < 0 || pid2 < 0 )	continue;

		// strandness
		char strand;		
		int s0 = 0;
		int s1 = 0;
		int s2 = 0;
		for(int k = 0; k < v.size(); k++)
		{
			if(v[k]->xs == '.') s0++;
			if(v[k]->xs == '+') s1++;
			if(v[k]->xs == '-') s2++;
		}
		if(s1 == 0 && s2 == 0) strand = '.';
		else if(s1 >= 1 && s2 >= 1) strand = '.';
		else if(s1 > s2) strand = '+';
		else strand = '-';

		assert(pexon_jset.find(pair<int, int>{pid1, pid2}) == pexon_jset.end());
		pexon_jset[{pid1, pid2}] = pair<int, char> {v.size(), strand};
	}

	return 0;
}


int bundle::locate_left_partial_exon(as_pos32 x)
{
	throw runtime_error("don't use locate left");
	/*
	assert(x.ale == "$");
	auto it = pmap_na.upper_bound(make_pair(x,x)); // p1>x or p1=x p2>x
	
	if (it == pmap_na.begin() ) 
	{
		if (it->first.first.p32 != x.p32) return -1;
	}
	else if (it == pmap_na.end()) it = prev(it);
	else if (it->first.first.p32 != x.p32) it = prev(it);
	
	assert(it->second >= 1);
	assert(it->second <= pexons.size());
	int k = it->second - 1;
	
	as_pos32 p1 = it->first.first;
	as_pos32 p2 = it->first.second;
	if(p2.leftsameto(x)) return -1;
	assert(p2.rightto(x));
	assert(p1.leftsameto(x));

	// if(x - p1.p32 > min_flank_length && p2.p32 - x < min_flank_length) k++;
	return k;
	*/
}

int bundle::locate_right_partial_exon(as_pos32 x)
{
	throw runtime_error("don't use locate right");
	/*
	assert(x.ale == "$");
	auto it = pmap_na.upper_bound(make_pair(x,x));
	if (it == pmap_na.begin()) return -1;
	it = prev(it);

	assert(it->second >= 1);
	assert(it->second <= pexons.size());
	int k = it->second - 1;
	
	as_pos32 p1 = it->first.first;
	as_pos32 p2 = it->first.second;
	if(p2.leftto(x)) return -1;
	assert(p2.rightsameto(x));
	assert(p1.leftto(x));

	// if(x - p1.p32 > min_flank_length && p2.p32 - x < min_flank_length) k++; //TODO: min_flank_length in pmap
	return k;
	*/
}

vector<int> bundle::align_hit(hit &h)
{
	bool b = true;
	vector<int> sp2;
	vector<int> v = decode_vlist(h.vlist);

	if(DEBUG_MODE_ON && print_bundle_detail)
	{
		cout << "align_hit, decode list v " ;
		for(int i: v) cout << i <<  " ";
		cout << endl;
	}

	if(v.size() == 0) return sp2;

	for(int k = 0; k < v.size(); k++)
	{
		region& r = br.regions[v[k]];
		if(r.pexons.size() == 0) 
		{
			b = false;
			break;
		}
		for(const partial_exon pe: r.pexons) sp2.push_back(pe.pid);
	}

	//TODO: min_flank_length filter

	vector<int> e;
	if(b == false) return e;
	else return sp2;
}

vector<int> bundle::align_fragment(fragment &fr)
{
	bool b = true;
	vector<int> sp2;
	vector<int> v = br.get_splices_region_index(fr);

	if(DEBUG_MODE_ON && print_bundle_detail)
	{
		cout << "align_fragment, decode list v " ;
		for(int i: v) cout << i <<  " ";
		cout << endl;
	}


	if(v.size() == 0) return sp2;

	for(int k = 0; k < v.size(); k++)
	{
		region& r = br.regions[v[k]];
		if(r.pexons.size() == 0) 
		{
			b = false;
			break;
		}
		for(const partial_exon pe: r.pexons) sp2.push_back(pe.pid);
	}

	//TODO: min_flank_length filter

	vector<int> e;
	if(b == false) return e;
	else return sp2;
}

int bundle::build_splice_graph(int mode)
{
	//FIXME: only two as vertices at the same sites. no "*" no "N", must be in reference, at most two.
	
	// build graph
	gr.clear();
	if (verbose >= 3) 
		cout << "splice graph build for bundle " << bb.chrm << ":" << bb.lpos << "-" << bb.rpos << " " <<bb.strand << " strand" << endl;
	// vertices: start, each region, end
	gr.add_vertex();
	vertex_info vi0;
	vi0.lpos = bb.lpos;
	vi0.rpos = bb.lpos;
	vi0.as_type = START_OR_SINK;
	gr.set_vertex_weight(0, 0);
	gr.set_vertex_info(0, vi0);

	int n_as = 0;

	for(int i = 0; i < pexons.size(); i++) // vertices for each (partial) exon
	{
		const partial_exon &r = pexons[i];
		int length = r.rpos.p32 - r.lpos.p32;
		assert(length >= 1);
		gr.add_vertex();
		if(mode == 1) gr.set_vertex_weight(i + 1, r.max < min_guaranteed_edge_weight ? min_guaranteed_edge_weight : r.max);
		if(mode == 2) gr.set_vertex_weight(i + 1, r.ave < min_guaranteed_edge_weight ? min_guaranteed_edge_weight : r.ave);
		vertex_info vi;
		vi.lpos = r.lpos;
		vi.rpos = r.rpos;
		vi.length = length;
		vi.gt = r.gt;
		
		// FIXME: not complete enumeration
		// UHPHASED_MONOVAR vs NS_NONVAR
		if (gt_as(r.gt))// FIXME: keep all var
		{
			vi.as_type = AS_DIPLOIDVAR;
			n_as += 1;
		} 
		else if (r.is_allelic() && r.gt == UNPHASED)
		{
			vi.as_type = AS_DIPLOIDVAR;
			n_as += 1;
		}
		else vi.as_type = NS_NONVAR;

		vi.stddev = r.dev;// < 1.0 ? 1.0 : r.dev;
		vi.regional = regional[i];
		vi.type = pexons[i].type;
		gr.set_vertex_info(i + 1, vi);
	}

	gr.add_vertex();
	vertex_info vin;
	vin.lpos = bb.rpos;
	vin.rpos = bb.rpos;
	vin.as_type = START_OR_SINK;
	gr.set_vertex_weight(pexons.size() + 1, 0);
	gr.set_vertex_info(pexons.size() + 1, vin);

	if(verbose >= 3) cout << "splice graph build junction edges\n";

	// edges: each junction => and e2w
	// vertics: assign as_type
	set<pair<int, int> > edge_set;
	for(const auto& jset_item: jset)
	{
		int  lpid   = jset_item.first.first;
		int  rpid   = jset_item.first.second;
		int  c      = jset_item.second.first;
		char strand = jset_item.second.second;

		if(lpid< 0 || rpid < 0) continue;

		const partial_exon &x = pexons[lpid];
		const partial_exon &y = pexons[rpid];

		edge_descriptor p = gr.add_edge(lpid + 1, rpid + 1);
		edge_set.insert(make_pair(lpid + 1, rpid + 1));

		assert(c >= 1);
		edge_info ei;
		ei.weight = c;
		ei.strand = strand;
		gr.set_edge_info(p, ei);
		gr.set_edge_weight(p, c);

		// assign as_type
		// FIXME: not complete enumeration
		// UHPHASED_MONOVAR vs NS_NONVAR
		if (!decompose_as_neighor)  // internal explore only, default false
		{
			vertex_info& vx = gr.vinf[lpid+1];
			vertex_info& vy = gr.vinf[rpid+1];
			if(vx.is_as_vertex())
			{
				if (!vy.is_as_vertex()) vy.as_type = AJ_NONVAR;
			}
			else if (vy.is_as_vertex())
			{
				if (!vx.is_as_vertex()) vx.as_type = AJ_NONVAR;
			}
		}
	}

	// edges: connecting start/end and pexons
	int ss = 0;
	int tt = pexons.size() + 1;
	for(int i = 0; i < pexons.size(); i++)
	{
		const partial_exon &r = pexons[i];

		if(r.ltype == START_BOUNDARY)
		{
			edge_descriptor p = gr.add_edge(ss, i + 1);
			// double w = r.ave;
			// if(i >= 1 && pexons[i - 1].rpos == r.lpos) w -= pexons[i - 1].ave; 
			// if(w < 1.0) w = 1.0;
			double w = min_guaranteed_edge_weight;
			if(mode == 1) w = r.max;
			if(mode == 2) w = r.ave;
			if(mode == 1 && i >= 1 && pexons[i - 1].rpos.p32 == r.lpos.p32) w -= pexons[i - 1].max;
			if(mode == 2 && i >= 1 && pexons[i - 1].rpos.p32 == r.lpos.p32) w -= pexons[i - 1].ave;
			if(w < min_guaranteed_edge_weight) w = min_guaranteed_edge_weight;

			gr.set_edge_weight(p, w);
			edge_info ei;
			ei.weight = w;
			gr.set_edge_info(p, ei);
		}

		if(r.rtype == END_BOUNDARY) 
		{
			edge_descriptor p = gr.add_edge(i + 1, tt);
			// double w = r.ave;
			// if(i < pexons.size() - 1 && pexons[i + 1].lpos == r.rpos) w -= pexons[i + 1].ave; 
			// if(w < 1.0) w = 1.0;
			double w = min_guaranteed_edge_weight;
			if(mode == 1) w = r.max;
			if(mode == 2) w = r.ave;
			if(mode == 1 && i < pexons.size() - 1 && pexons[i + 1].lpos.p32 == r.rpos.p32) w -= pexons[i + 1].max;
			if(mode == 2 && i < pexons.size() - 1 && pexons[i + 1].lpos.p32 == r.rpos.p32) w -= pexons[i + 1].ave;
			if(w < min_guaranteed_edge_weight) w = min_guaranteed_edge_weight;
			gr.set_edge_weight(p, w);
			edge_info ei;
			ei.weight = w;
			gr.set_edge_info(p, ei);
		}
	}

	// FIXME: given edges are build from bridged paths/aligned_itv all adjacent pexons should already have such edges. Need check
	// edges: connecting adjacent pexons => e2w 
	/*
	for(int i = 0; i < (int)(pexons.size()) - 1; i++)
	{
		const partial_exon &x = pexons[i];
		int k = 1;
		for (; k < pexons.size() - 1 - i; ++k)
		{
			const partial_exon &z = pexons[i + k];
			if (x.lpos.samepos(z.lpos)) continue;
			else break;
		}
		
		while (i+k < pexons.size() && x.rpos.samepos(pexons[i+k].lpos))
		{
			// TODO: find all k, might be more than one k
			const partial_exon &y = pexons[i+k]; // y is first non-allele after x
			assert(!x.lpos.samepos(y.lpos));
			assert(x.rpos.p32 == y.lpos.p32);
			
			int xd = gr.out_degree(i + 1);
			int yd = gr.in_degree(i + k + 1);
			// double wt = (xd < yd) ? x.ave : y.ave;
			double wt = min_guaranteed_edge_weight;
			if(mode == 1) wt = (xd < yd) ? x.max: y.max;
			if(mode == 2) wt = (xd < yd) ? x.ave: y.ave;
			//int32_t xr = compute_overlap(mmap, x.rpos - 1);						
			//int32_t yl = compute_overlap(mmap, y.lpos);
			//double wt = xr < yl ? xr : yl;
			if (edge_set.find(make_pair(i + 1, i + k + 1)) == edge_set.end() ) 		// is edge present in junction
			{
				edge_descriptor p = gr.add_edge(i + 1, i + k + 1);
				// double w = (wt < 1.0) ? 1.0 : wt;
				double w = (wt < min_guaranteed_edge_weight) ? min_guaranteed_edge_weight : wt;
				gr.set_edge_weight(p, w);
				edge_info ei;
				ei.weight = w;
				gr.set_edge_info(p, ei);
			}
			k++;
		}
	}
	*/

	gr.strand = bb.strand;
	gr.chrm = bb.chrm;

	return 0;
}

int bundle::revise_splice_graph()
{
	bool b = false;
	while(true)
	{
		b = tackle_false_boundaries();
		if(b == true) continue;

		// b = extend_boundaries();
		b = remove_false_boundaries();
		if(b == true) continue;

		b = remove_inner_boundaries();
		if(b == true) continue;

		b = remove_small_exons();//FIXME:
		// if(b == true) refine_splice_graph();
		if(b == true) continue;

		b = remove_intron_contamination();
		if(b == true) continue;

		b = remove_small_junctions();//FIXME:
		if(b == true) refine_splice_graph();
		if(b == true) continue;

		// b = keep_surviving_edges();
		b = extend_start_boundaries();
		if(b == true) continue;

		b = extend_end_boundaries();
		if(b == true) continue;

		b = extend_boundaries();
		if(b == true) refine_splice_graph();
		if(b == true) continue;

		// b = remove_intron_contamination();
		b = keep_surviving_edges();
		if(b == true) refine_splice_graph();
		if(b == true) continue;

		break;
	}

	refine_splice_graph();
	return 0;
}

int bundle::refine_splice_graph()
{
	while(true)
	{
		bool b = false;
		for(int i = 1; i < gr.num_vertices() - 1; i++)
		{
			if(gr.degree(i) == 0) continue;
			if(gr.in_degree(i) >= 1 && gr.out_degree(i) >= 1) continue;
			gr.clear_vertex(i);
			b = true;
		}
		if(b == false) break;
	}
	return 0;
}

bool bundle::extend_start_boundaries()
{
	bool flag = false;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		PEB p = gr.edge(0, i);
		if(p.second == true) continue;

		double wv = gr.get_vertex_weight(i);
		double we = 0;
		PEEI pei = gr.in_edges(i);
		for(edge_iterator it = pei.first; it != pei.second; it++)
		{
			we += gr.get_edge_weight(*it);
		}

		if(wv < we || wv < 10 * we * we + 10) continue;

		edge_descriptor ee = gr.add_edge(0, i);
		gr.set_edge_weight(ee, wv - we);
		gr.set_edge_info(ee, edge_info());

		vertex_info vi = gr.get_vertex_info(i);
		if(verbose >= 2) printf("extend start boundary: vertex = %d, wv = %.2lf, we = %.2lf, pos = %d%s\n", i, wv, we, vi.lpos.p32, vi.lpos.ale.c_str());

		flag = true;
	}
	return flag;
}

bool bundle::extend_end_boundaries()
{
	bool flag = false;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		PEB p = gr.edge(i, gr.num_vertices() - 1);
		if(p.second == true) continue;

		double wv = gr.get_vertex_weight(i);
		double we = 0;
		PEEI pei = gr.out_edges(i);
		for(edge_iterator it = pei.first; it != pei.second; it++)
		{
			we += gr.get_edge_weight(*it);
		}

		if(wv < we || wv < 10 * we * we + 10) continue;

		edge_descriptor ee = gr.add_edge(i, gr.num_vertices() - 1);
		gr.set_edge_weight(ee, wv - we);
		gr.set_edge_info(ee, edge_info());

		vertex_info vi = gr.get_vertex_info(i);
		if(verbose >= 2) printf("extend end boundary: vertex = %d, wv = %.2lf, we = %.2lf, pos = %d%s\n", i, wv, we, vi.rpos.p32, vi.rpos.ale.c_str());

		flag = true;
	}
	return flag;
}

bool bundle::extend_boundaries()
{
	edge_iterator it1, it2;
	PEEI pei;
	for(pei = gr.edges(), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
	{
		edge_descriptor e = (*it1);
		int s = e->source();
		int t = e->target();
		int32_t p = gr.get_vertex_info(t).lpos - gr.get_vertex_info(s).rpos;
		double we = gr.get_edge_weight(e);
		double ws = gr.get_vertex_weight(s);
		double wt = gr.get_vertex_weight(t);

		if(p <= 0) continue;
		if(s == 0) continue;
		if(t == gr.num_vertices() - 1) continue;

		bool b = false;
		if(gr.out_degree(s) == 1 && ws >= 10.0 * we * we + 10.0) b = true;
		if(gr.in_degree(t) == 1 && wt >= 10.0 * we * we + 10.0) b = true;

		if(b == false) continue;

		if(gr.out_degree(s) == 1)
		{
			edge_descriptor ee = gr.add_edge(s, gr.num_vertices() - 1);
			gr.set_edge_weight(ee, ws);
			gr.set_edge_info(ee, edge_info());
		}
		if(gr.in_degree(t) == 1)
		{
			edge_descriptor ee = gr.add_edge(0, t);
			gr.set_edge_weight(ee, wt);
			gr.set_edge_info(ee, edge_info());
		}

		gr.remove_edge(e);

		return true;
	}

	return false;
}

VE bundle::compute_maximal_edges()
{
	typedef pair<double, edge_descriptor> PDE;
	vector<PDE> ve;

	undirected_graph ug;
	edge_iterator it1, it2;
	PEEI pei;
	for(int i = 0; i < gr.num_vertices(); i++) ug.add_vertex();
	for(pei = gr.edges(), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
	{
		edge_descriptor e = (*it1);
		double w = gr.get_edge_weight(e);
		int s = e->source();
		int t = e->target();
		if(s == 0) continue;
		if(t == gr.num_vertices() - 1) continue;
		ug.add_edge(s, t);
		ve.push_back(PDE(w, e));
	}

	vector<int> vv = ug.assign_connected_components();

	sort(ve.begin(), ve.end());

	for(int i = 1; i < ve.size(); i++) assert(ve[i - 1].first <= ve[i].first);

	VE x;
	set<int> sc;
	for(int i = ve.size() - 1; i >= 0; i--)
	{
		edge_descriptor e = ve[i].second;
		double w = gr.get_edge_weight(e);
		if(w < 1.5) break;
		int s = e->source();
		int t = e->target();
		if(s == 0) continue;
		if(t == gr.num_vertices()) continue;
		int c1 = vv[s];
		int c2 = vv[t];
		assert(c1 == c2);
		if(sc.find(c1) != sc.end()) continue;
		x.push_back(e);
		sc.insert(c1);
	}
	return x;
}

bool bundle::keep_surviving_edges() //FIXME:
{
	set<int> sv1;
	set<int> sv2;
	SE se;
	edge_iterator it1, it2;
	PEEI pei;
	for(pei = gr.edges(), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
	{
		int s = (*it1)->source();
		int t = (*it1)->target();
		double w = gr.get_edge_weight(*it1);
		int32_t p1 = gr.get_vertex_info(s).rpos;
		int32_t p2 = gr.get_vertex_info(t).lpos;
		if(w < min_surviving_edge_weight) continue;
		se.insert(*it1);
		sv1.insert(t);
		sv2.insert(s);
	}

	VE me = compute_maximal_edges();
	for(int i = 0; i < me.size(); i++)
	{
		edge_descriptor ee = me[i];
		se.insert(ee);
		sv1.insert(ee->target());
		sv2.insert(ee->source());
	}

	while(true)
	{
		bool b = false;
		for(SE::iterator it = se.begin(); it != se.end(); it++)
		{
			edge_descriptor e = (*it);
			int s = e->source(); 
			int t = e->target();
			if(sv1.find(s) == sv1.end() && s != 0)
			{
				edge_descriptor ee = gr.max_in_edge(s);
				assert(ee != null_edge);
				assert(se.find(ee) == se.end());
				se.insert(ee);
				sv1.insert(s);
				sv2.insert(ee->source());
				b = true;
			}
			if(sv2.find(t) == sv2.end() && t != gr.num_vertices() - 1)
			{
				edge_descriptor ee = gr.max_out_edge(t);
				assert(ee != null_edge);
				assert(se.find(ee) == se.end());
				se.insert(ee);
				sv1.insert(ee->target());
				sv2.insert(t);
				b = true;
			}
			if(b == true) break;
		}
		if(b == false) break;
	}

	VE ve;
	for(pei = gr.edges(), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
	{
		if(se.find(*it1) != se.end()) continue;
		ve.push_back(*it1);
	}

	for(int i = 0; i < ve.size(); i++)
	{
		if(verbose >= 2) printf("remove edge (%d, %d), weight = %.2lf\n", ve[i]->source(), ve[i]->target(), gr.get_edge_weight(ve[i]));
		gr.remove_edge(ve[i]);
	}

	if(ve.size() >= 1) return true;
	else return false;
}

bool bundle::remove_small_exons()
{
	bool flag = false;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		vertex_info vi = gr.get_vertex_info(i);
		if(vi.type == EMPTY_VERTEX) continue;

		bool b = true;
		edge_iterator it1, it2;
		PEEI pei;
		int32_t p1 = gr.get_vertex_info(i).lpos;
		int32_t p2 = gr.get_vertex_info(i).rpos;

		if(p2 - p1 >= min_exon_length) continue;
		if(gr.degree(i) <= 0) continue;

		for(pei = gr.in_edges(i), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
		{
			edge_descriptor e = (*it1);
			int s = e->source();
			//if(gr.out_degree(s) <= 1) b = false;
			if(s != 0 && gr.get_vertex_info(s).rpos == p1) b = false;
			if(b == false) break;
		}
		for(pei = gr.out_edges(i), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
		{
			edge_descriptor e = (*it1);
			int t = e->target();
			//if(gr.in_degree(t) <= 1) b = false;
			if(t != gr.num_vertices() - 1 && gr.get_vertex_info(t).lpos == p2) b = false;
			if(b == false) break;
		}

		if(b == false) continue;

		// only consider boundary small exons
		if(gr.edge(0, i).second == false && gr.edge(i, gr.num_vertices() - 1).second == false) continue;

		//gr.clear_vertex(i);
		if(verbose >= 2) printf("remove small exon: length = %d, pos = %d-%d\n", p2 - p1, p1, p2);
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(i, vi);

		flag = true;
	}
	return flag;
}

bool bundle::remove_small_junctions()
{
	SE se;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		if(gr.degree(i) <= 0) continue;

		bool b = true;
		edge_iterator it1, it2;
		PEEI pei;
		int32_t p1 = gr.get_vertex_info(i).lpos;
		int32_t p2 = gr.get_vertex_info(i).rpos;
		double wi = gr.get_vertex_weight(i);

		// compute max in-adjacent edge
		double ws = 0;
		for(pei = gr.in_edges(i), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
		{
			edge_descriptor e = (*it1);
			int s = e->source();
			double w = gr.get_vertex_weight(s);
			if(s == 0) continue;
			if(gr.get_vertex_info(s).rpos != p1) continue;
			if(w < ws) continue;
			ws = w;
		}

		// remove small in-junction
		for(pei = gr.in_edges(i), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
		{
			edge_descriptor e = (*it1);
			int s = e->source();
			double w = gr.get_edge_weight(e);
			if(s == 0) continue;
			if(gr.get_vertex_info(s).rpos == p1) continue;
			if(ws < 2.0 * w * w + 18.0) continue;
			if(wi < 2.0 * w * w + 18.0) continue;

			se.insert(e);
		}

		// compute max out-adjacent edge
		double wt = 0;
		for(pei = gr.out_edges(i), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
		{
			edge_descriptor e = (*it1);
			int t = e->target();
			double w = gr.get_vertex_weight(t);
			if(t == gr.num_vertices() - 1) continue;
			if(gr.get_vertex_info(t).lpos != p2) continue;
			if(w < wt) continue;
			wt = w;
		}

		// remove small in-junction
		for(pei = gr.out_edges(i), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
		{
			edge_descriptor e = (*it1);
			double w = gr.get_edge_weight(e);
			int t = e->target();
			if(t == gr.num_vertices() - 1) continue;
			if(gr.get_vertex_info(t).lpos == p2) continue;
			if(ws < 2.0 * w * w + 18.0) continue;
			if(wi < 2.0 * w * w + 18.0) continue;

			se.insert(e);
		}

	}

	if(se.size() <= 0) return false;

	for(SE::iterator it = se.begin(); it != se.end(); it++)
	{
		edge_descriptor e = (*it);
		if(verbose >= 2) 
		{
			vertex_info v1 = gr.get_vertex_info(e->source());
			vertex_info v2 = gr.get_vertex_info(e->target());
			printf("remove small junction: length = %d, pos = %d%s-%d%s\n", v2.lpos - v1.rpos, v2.lpos.p32, v2.lpos.ale.c_str(), v1.rpos.p32, v1.rpos.ale.c_str());
		}
		gr.remove_edge(e);
	}

	return true;
}

bool bundle::remove_inner_boundaries()
{
	bool flag = false;
	int n = gr.num_vertices() - 1;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		vertex_info vi = gr.get_vertex_info(i);
		if(vi.type == EMPTY_VERTEX) continue;
		
		if(gr.in_degree(i) != 1) continue;
		if(gr.out_degree(i) != 1) continue;

		PEEI pei = gr.in_edges(i);
		edge_iterator it1 = pei.first, it2 = pei.second;
		edge_descriptor e1 = (*it1);

		pei = gr.out_edges(i);
		it1 = pei.first;
		it2 = pei.second;
		edge_descriptor e2 = (*it1);

		int s = e1->source();
		int t = e2->target();

		if(s != 0 && t != n) continue;
		if(s != 0 && gr.out_degree(s) == 1) continue;
		if(t != n && gr.in_degree(t) == 1) continue;

		if(vi.stddev >= 0.01) continue;

		if(verbose >= 2) printf("remove inner boundary: vertex = %d, weight = %.2lf, length = %d, pos = %d-%d\n",
				i, gr.get_vertex_weight(i), vi.length, vi.lpos.p32, vi.rpos.p32);

		// gr.clear_vertex(i);
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(i, vi);
		flag = true;
	}
	return flag;
}

bool bundle::remove_intron_contamination()
{
	bool flag = false;
	for(int i = 1; i < gr.num_vertices(); i++)
	{
		vertex_info vi = gr.get_vertex_info(i);
		if(vi.type == EMPTY_VERTEX) continue;
		
		if(gr.in_degree(i) != 1) continue;
		if(gr.out_degree(i) != 1) continue;

		edge_iterator it1, it2;
		PEEI pei = gr.in_edges(i);
		it1 = pei.first;
		edge_descriptor e1 = (*it1);
		pei = gr.out_edges(i);
		it1 = pei.first;
		edge_descriptor e2 = (*it1);
		int s = e1->source();
		int t = e2->target();
		double wv = gr.get_vertex_weight(i);

		if(s == 0) continue;
		if(t == gr.num_vertices() - 1) continue;
		if(gr.get_vertex_info(s).rpos != vi.lpos) continue;
		if(gr.get_vertex_info(t).lpos != vi.rpos) continue;

		PEB p = gr.edge(s, t);
		if(p.second == false) continue;

		edge_descriptor ee = p.first;
		double we = gr.get_edge_weight(ee);

		if(wv > we) continue;
		if(wv > max_intron_contamination_coverage) continue;

		if(verbose >= 2) printf("clear intron contamination %d, weight = %.2lf, length = %d, edge weight = %.2lf\n", i, wv, vi.length, we);

		// gr.clear_vertex(i);
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(i, vi);

		flag = true;
	}
	return flag;
}

// add by Mingfu -- to use paired-end reads to remove false boundaries
bool bundle::remove_false_boundaries()
{
	map<int, int> fb1;		// end
	map<int, int> fb2;		// start
	for(int i = 0; i < br.fragments.size(); i++)
	{
		fragment &fr = br.fragments[i];
		if(fr.paths.size() == 1 && fr.paths[0].type == 1) continue;
		//if(fr.h1->bridged == true || fr.h2->bridged == true) continue;

		// only use uniquely aligned reads
		//if(fr.h1->nh >= 2 || fr.h2->nh >= 2) continue;
		if(br.breads.find(fr.h1->qname) != br.breads.end()) continue;

		// calculate actual length
		vector<int> v = align_fragment(fr);
		
		if(v.size() <= 1) continue;

		int32_t tlen = 0;
		int32_t offset1 = (fr.lpos - pexons[v.front()].lpos);
		int32_t offset2 = (pexons[v.back()].rpos - fr.rpos);
		for(int i = 0; i < v.size(); i++)
		{
			int32_t l = pexons[v[i]].rpos - pexons[v[i]].lpos;
			tlen += l;
		}
		tlen -= offset1;
		tlen -= offset2;

		int u1 = gr.locate_vertex(fr.h1->rpos - 1);
		int u2 = gr.locate_vertex(fr.h2->pos);

		if(u1 < 0 || u2 < 0) continue;
		if(u1 >= u2) continue;

		vertex_info v1 = gr.get_vertex_info(u1);
		vertex_info v2 = gr.get_vertex_info(u2);

		int types = 0;
		int32_t lengths = 0;
		for(int k = 0; k < fr.paths.size(); k++) types += fr.paths[k].type;
		for(int k = 0; k < fr.paths.size(); k++) lengths += fr.paths[k].length;

		bool use = true;
		if(fr.paths.size() == 1 && types == 2 && tlen > 10000) use = false;
		//if(fr.paths.size() == 1 && types == 2 && lengths <= 1.5 * insertsize_high) use = false;
		//if(fr.paths.size() == 1 && types == 2 && tlen <= 1.5 * insertsize_high) use = false;
		//if(fr.paths.size() == 1 && types == 2 && lengths <= 2 * tlen) use = false;

		if(verbose >= 2) printf("%s: u1 = %d, %d%s-%d%s, u2 = %d, %d%s-%d%s, h1.rpos = %d, h2.lpos = %d, #bridging = %lu, types = %d, lengths = %d, tlen = %d, use = %c\n", 
				fr.h1->qname.c_str(), u1, v1.lpos.p32, v1.lpos.ale.c_str(), v1.rpos.p32, v1.rpos.ale.c_str(), u2, v2.lpos.p32, v2.lpos.ale.c_str(), v2.rpos.p32, v2.rpos.ale.c_str(), fr.h1->rpos, fr.h2->pos, fr.paths.size(), types, lengths, tlen, use ? 'T' : 'F');

		if(use == false) continue;

		//if(gr.get_vertex_info(u1).rpos == fr.h1->rpos)
		{
			if(fb1.find(u1) != fb1.end()) fb1[u1]++;
			else fb1.insert(make_pair(u1, 1));
		}

		//if(gr.get_vertex_info(u2).lpos == fr.h2->pos)
		{
			if(fb2.find(u2) != fb2.end()) fb2[u2]++;
			else fb2.insert(make_pair(u2, 1));
		}
	}

	bool b = false;
	for(auto &x : fb1)
	{
		PEB p = gr.edge(x.first, gr.num_vertices() - 1);
		vertex_info vi = gr.get_vertex_info(x.first);
		if(vi.type == EMPTY_VERTEX) continue;
		if(p.second == false) continue;
		double w = gr.get_vertex_weight(x.first);
		double z = log(1 + w) / log(1 + x.second);
		double s = log(1 + w) - log(1 + x.second);
		if(s > 1.5) continue;
		if(verbose >= 2) printf("detect false end boundary %d with %d reads, vertex = %d, w = %.2lf, type = %d, z = %.2lf, s = %.2lf\n", vi.rpos.p32, x.second, x.first, w, vi.type, z, s); 
		//gr.remove_edge(p.first);
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(x.first, vi);
		b = true;
	}

	for(auto &x : fb2)
	{
		PEB p = gr.edge(0, x.first);
		vertex_info vi = gr.get_vertex_info(x.first);
		if(vi.type == EMPTY_VERTEX) continue;
		if(p.second == false) continue;
		double w = gr.get_vertex_weight(x.first);
		double z = log(1 + w) / log(1 + x.second);
		double s = log(1 + w) - log(1 + x.second);
		if(s > 1.5) continue;
		if(verbose >= 2) printf("detect false start boundary %d with %d reads, vertex = %d, w = %.2lf, type = %d, z = %.2lf, s = %.2lf\n", vi.lpos.p32, x.second, x.first, w, vi.type, z, s); 
		//gr.remove_edge(p.first);
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(x.first, vi);
		b = true;
	}
	return b;
}

bool bundle::tackle_false_boundaries()
{
	bool b = false;
	vector<int> points(pexons.size(), 0);
	for(int k = 0; k < br.fragments.size(); k++)
	{
		fragment &fr = br.fragments[k];

		if(fr.paths.size() != 1) continue;
		if(fr.paths[0].type != 2) continue;
		if(br.breads.find(fr.h1->qname) != br.breads.end()) continue;

		vector<int> v = align_fragment(fr);
		if(v.size() <= 1) continue;

		int32_t offset1 = (fr.lpos - pexons[v.front()].lpos);
		int32_t offset2 = (pexons[v.back()].rpos - fr.rpos);

		int32_t tlen = 0;
		for(int i = 0; i < v.size(); i++)
		{
			int32_t l = pexons[v[i]].rpos - pexons[v[i]].lpos;
			tlen += l;
		}
		tlen -= offset1;
		tlen -= offset2;

		// print
		//fr.print(99);
		if(verbose >= 2) printf("break fragment %s: total-length = %d, bridge-length = %d\n", fr.h1->qname.c_str(), tlen, fr.paths[0].length);
		/*
		for(int i = 0; i < v.size(); i++)
		{
			int32_t l = pexons[v[i]].rpos - pexons[v[i]].lpos;
			if(i == 0) l -= offset1;
			if(i == v.size() - 1) l -= offset2;
			printf(" vertex %d: length = %d, region = %d-%d -> %d\n", v[i], l, pexons[v[i]].lpos, pexons[v[i]].rpos, pexons[v[i]].rpos - pexons[v[i]].lpos);
		}
		*/

		if(tlen < insertsize_low / 2.0) continue;
		if(tlen > insertsize_high * 2.0) continue;
		if(tlen >= fr.paths[0].length) continue;

		for(int i = 0; i < v.size() - 1; i++)
		{
			partial_exon &px = pexons[v[i + 0]];
			partial_exon &py = pexons[v[i + 1]];
			if(px.rtype == END_BOUNDARY) 
			{
				if(verbose >= 2) printf("break ending vertex %d, pos = %d%s\n", v[i], px.rpos.p32, px.rpos.ale.c_str());
				points[v[i + 0]] += 1;
			}
			if(py.ltype == START_BOUNDARY) 
			{
				if(verbose >= 2) printf("break starting vertex %d, pos = %d%s\n", v[i + 1], py.lpos.p32, py.lpos.ale.c_str());
				points[v[i + 1]] += 1;
			}
		}
	}

	for(int k = 0; k < points.size(); k++)
	{
		if(points[k] <= 0) continue;
		vertex_info vi = gr.get_vertex_info(k + 1);
		if(vi.type == EMPTY_VERTEX) continue;
		PEB p = gr.edge(k + 1, gr.num_vertices() - 1);
		if(p.second == false) continue;
		double w = gr.get_vertex_weight(k + 1);
		double z = log(1 + w) / log(1 + points[k]);
		double s = log(1 + w) - log(1 + points[k]);
		if(verbose >= 2) printf("tackle false end boundary %d with %d reads, vertex = %d, w = %.2lf, z = %.2lf, s = %.2lf\n", pexons[k].rpos.p32, points[k], k + 1, w, z, s);
		if(s > 1.5) continue;
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(k + 1, vi);
		b = true;
	}

	for(int k = 0; k < points.size(); k++)
	{
		if(points[k] <= 0) continue;
		vertex_info vi = gr.get_vertex_info(k + 1);
		if(vi.type == EMPTY_VERTEX) continue;
		PEB p = gr.edge(0, k + 1);
		if(p.second == false) continue;
		double w = gr.get_vertex_weight(k + 1);
		double z = log(1 + w) / log(1 + points[k]);
		double s = log(1 + w) - log(1 + points[k]);
		if(verbose >= 2) printf("tackle false start boundary %d with %d reads, vertex = %d, w = %.2lf, z = %.2lf, s = %.2lf\n", pexons[k].lpos.p32, points[k], k + 1, w, z, s);
		if(s > 1.5) continue;
		vi.type = EMPTY_VERTEX;
		gr.set_vertex_info(k + 1, vi);
		b = true;
	}

	return b;
}

int bundle::print(int index)
{
	printf("Bundle %d: ", index);

	// statistic xs
	int n0 = 0, np = 0, nq = 0;
	for(int i = 0; i < bb.hits.size(); i++)
	{
		if(bb.hits[i].xs == '.') n0++;
		if(bb.hits[i].xs == '+') np++;
		if(bb.hits[i].xs == '-') nq++;
	}

	printf("tid = %d, #hits = %lu, #partial-exons = %lu, range = %s:%d-%d, orient = %c (%d, %d, %d)\n",
			bb.tid, bb.hits.size(), pexons.size(), bb.chrm.c_str(), bb.lpos, bb.rpos, bb.strand, n0, np, nq);

	if(verbose <= 1) return 0;

	// print hits
	for(int i = 0; i < bb.hits.size(); i++) bb.hits[i].print();

	// print fmap
	/*
	for(JIMI it = fmap.begin(); it != fmap.end(); it++)
	{
		printf("bundle.fmap %d: jmap [%d%s, %d%s) -> %d\n", 
			index, lower(it->first).p32, lower(it->first).ale.c_str(), upper(it->first).p32, upper(it->first).ale.c_str(), it->second);
	}
	*/

	// print regions
	const vector<region>& regions = br.regions;
	for(int i = 0; i < regions.size(); i++)
	{
		regions[i].print(i);	
	}

	// print partial exons
	for(int i = 0; i < pexons.size(); i++)
	{
		pexons[i].print(i);
	}

	// print jset 
	for(auto i: jset)
	{
		int pid1 = i.first.first;
		int pid2 = i.first.second;
		int c = i.second.first;
		char s = i.second.second;
		cout << "jset: " << pid1 << "-" << pid2 << " " << s << " strand, counts = " << c << endl;
	}

	printf("\n");

	return 0;
}

int bundle::build_hyper_set()
{
	map<vector<int>, int> m;

	for(int k = 0; k < br.fragments.size(); k++)
	{
		fragment &fr = br.fragments[k];
	
		if(fr.type != 0) continue;	// note by Qimin, skip if not paired-end fragments

		if(fr.h1->paired != true) printf("error type: %d\n", fr.type);
		assert(fr.h1->paired == true);
		assert(fr.h2->paired == true);

		if(fr.paths.size() != 1) continue;
		if(fr.paths[0].type != 1) continue;

		//if(fr.h1->bridged == false) continue;
		//if(fr.h2->bridged == false) continue;

		vector<int> v = align_fragment(fr);
		
		if(m.find(v) == m.end()) m.insert(pair<vector<int>, int>(v, fr.cnt));
		else m[v] += fr.cnt;
	}
	
	// note by Qimin, bridge umi-linked fragments into one single long path  //TODO:
	for(int k = 0; k < br.umiLink.size(); k++)
	{
		vector<int> v;
		v.clear();

		int cnt = 0;

		// if only one fr, no need to bridge into longer one
		if(br.umiLink[k].size() == 1)
		{
			fragment &fr = br.fragments[(br.umiLink[k][0])];

			if(fr.paths.size() != 1) continue;

			// TODO: "bridged" may not be correct
			if(fr.h1->bridged == false) continue;
			if(fr.h2->bridged == false) continue;

			v = align_fragment(fr);
			if(fr.paths.size() != 1 || fr.paths[0].type != 1) v.clear();

			if(m.find(v) == m.end()) m.insert(pair<vector<int>, int>(v, fr.cnt));
			else m[v] += fr.cnt;

			continue;
		}

		// if multiple fr in umi-link, bridge into one single long path
		for(int kk = 0; kk < br.umiLink[k].size(); kk++)
		{
			fragment &fr = br.fragments[(br.umiLink[k][kk])];

			// if unbridge, then trucate and add to m
			if(fr.paths.size() != 1 || fr.h1->bridged == false || fr.h2->bridged == false)
			{
				if(v.size() > 0)
				{
					if(m.find(v) == m.end()) m.insert(pair<vector<int>, int>(v, cnt));
					else m[v] += cnt;
				}

				v.clear();
				cnt = 0;

				continue;
			}

			// otherwise, add and merge cur_v to v
			vector<int> cur_v = align_fragment(fr);
			if(fr.paths.size() != 1 || fr.paths[0].type != 1) cur_v.clear();

			if(cur_v.size()==0)
			{
				if(v.size() > 0)
				{
					if(m.find(v) == m.end()) m.insert(pair<vector<int>, int>(v, cnt));
					else m[v] += cnt;
				}

				v.clear();
				cnt = 0;

				continue;

			}
			cnt += fr.cnt;

			v.insert(v.end(), cur_v.begin(), cur_v.end());
			sort(v.begin(), v.end());
			vector<int>::iterator iter = unique(v.begin(),v.end());
			v.erase(iter,v.end());
		}

		if(v.size() > 0)
		{
			/*
			printf("v = ");
			for(int ii = 0; ii < v.size(); ii++)
			{
				printf("%d ", v[ii]);
			}
			printf("\n");
			*/

			if(m.find(v) == m.end()) m.insert(pair<vector<int>, int>(v, cnt));
			else m[v] += cnt;
		}
	}
	
	for(int k = 0; k < bb.hits.size(); k++)
	{
		hit &h = bb.hits[k];

		// bridged used here, but maybe okay
		if(h.bridged == true) continue;

		vector<int> v = align_hit(h);
		
		if(m.find(v) == m.end()) m.insert(pair<vector<int>, int>(v, 1));
		else m[v] += 1;
	}

	/*
	if(DEBUG_MODE_ON && print_bundle_detail) 
	{
		cout << "bundle::build_hyper_set() get path from br.fragments; stage 2; size = " << m.size() << endl;
		for(const auto & mvii:m)
		{
			const vector<int>& vi = mvii.first;
			int i = mvii.second;
			cout << "\t";
			for(int j: vi)
			{
				cout << j << ", ";
			}
			cout << ": " << i << ";" <<  endl;
		}
	}
	*/

	hs.clear();
	for(map<vector<int>, int>::iterator it = m.begin(); it != m.end(); it++)
	{
		const vector<int> &v = it->first;
		int c = it->second;
		if(v.size() >= 2) hs.add_node_list(v, c);
	}

	if(DEBUG_MODE_ON && print_bundle_detail) {cout << "build_hyper_set completed. print hs." << endl; hs.print();}

	return 0;
}
