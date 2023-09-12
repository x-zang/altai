/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Scallop2
(c) 2021 by  Qimin Zhang, Mingfu Shao, and The Pennsylvania State University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#include <algorithm>
#include <cstdio>
#include <cassert>
#include <sstream>
#include <iostream>
#include <map>
#include <cstring>

#include "config.h"
#include "genome.h"
#include "assembler.h"
#include "bundle.h"
#include "scallop.h"
#include "sgraph_compare.h"
#include "super_graph.h"
#include "filter.h"
#include "vcf_data.h"
#include "phaser.h"
#include "util.h"

assembler::assembler()
{
    sfn = sam_open(input_file.c_str(), "r");
    hdr = sam_hdr_read(sfn);
    b1t = bam_init1();
	hid = 0;
	index = 0;
	terminate = false;
	qlen = 0;
	qcnt = 0;
	vmap_chrm = "";  // reset vcf pointers from previewer
}

assembler::~assembler()
{
    bam_destroy1(b1t);
    bam_hdr_destroy(hdr);
    sam_close(sfn);
	fai_destroy(fai);
}

int assembler::assemble()
{
    while(sam_read1(sfn, hdr, b1t) >= 0)
	{
		if(terminate == true) return 0;

		bam1_core_t &p = b1t->core;

		if(p.tid < 0) continue;
		if((p.flag & 0x4) >= 1) continue;										// read is not mapped
		if((p.flag & 0x100) >= 1 && use_second_alignment == false) continue;	// secondary alignment
		if(p.n_cigar > max_num_cigar) continue;									// ignore hits with more than max-num-cigar types
		if(p.qual < min_mapping_quality) continue;								// ignore hits with small quality
		if(p.n_cigar < 1) continue;												// should never happen


		char buf[1024];
		strcpy(buf, hdr->target_name[p.tid]);

		hit ht(b1t, string(buf), hid++);
		ht.set_tags(b1t);
		ht.set_strand();
		
		qlen += ht.qlen;
		qcnt += 1;

		// truncate
		if(ht.tid != bb1.tid || ht.pos > bb1.rpos + min_bundle_gap)
		{
			if (bb1.hits.size() >= 1) pool.push_back(bb1);
			bb1.clear();
		}
		if(ht.tid != bb2.tid || ht.pos > bb2.rpos + min_bundle_gap)
		{
			if(bb2.hits.size() >= 1) pool.push_back(bb2);
			bb2.clear();
		}

		// process
		process(batch_bundle_size);

		//printf("read strand = %c, xs = %c, ts = %c\n", ht.strand, ht.xs, ht.ts);

		// add hit
		if(uniquely_mapped_only == true && ht.nh != 1) continue;
		if(library_type != UNSTRANDED && ht.strand == '+' && ht.xs == '-') continue;
		if(library_type != UNSTRANDED && ht.strand == '-' && ht.xs == '+') continue;
		if(library_type != UNSTRANDED && ht.strand == '.' && ht.xs != '.') ht.strand = ht.xs;
		if(library_type != UNSTRANDED && ht.strand == '+') bb1.add_hit(ht);
		if(library_type != UNSTRANDED && ht.strand == '-') bb2.add_hit(ht);
		if(library_type == UNSTRANDED && ht.xs == '.') bb1.add_hit(ht);
		if(library_type == UNSTRANDED && ht.xs == '.') bb2.add_hit(ht);
		if(library_type == UNSTRANDED && ht.xs == '+') bb1.add_hit(ht);
		if(library_type == UNSTRANDED && ht.xs == '-') bb2.add_hit(ht);
	}

	pool.push_back(bb1);
	pool.push_back(bb2);
	process(0);

	assign_RPKM();
	trsts.insert(trsts.end(), non_full_trsts.begin(), non_full_trsts.end()); //FIXME: TODO:
	if(DEBUG_MODE_ON && trsts.size() < 1) throw runtime_error("No AS transcript found!");

	filter ft(trsts);
	ft.merge_single_exon_transcripts();
	trsts = ft.trs;

	filter ft1(non_full_trsts);
	ft1.merge_single_exon_transcripts();
	non_full_trsts = ft1.trs;

	write();
	
	cout << "Altai finished running." << endl;

	return 0;
}

int assembler::process(int n)
{
	if(pool.size() < n) return 0;
	for(int i = 0; i < pool.size(); i++)
	{
		try 
		{
			bundle_base &bb = pool[i];
			bb.buildbase();

			if(verbose >= 3) printf("bundle %d has %lu reads\n", index, bb.hits.size());

			int cnt1 = 0;
			int cnt2 = 0;
			for(int k = 0; k < bb.hits.size(); k++)
			{
				//counts += (1 + bb.hits[k].spos.size());
				if(bb.hits[k].spos.size() >= 1) cnt1 ++;
				else cnt2++;
			}
			if(cnt1 + cnt2 < min_num_hits_in_bundle) continue;

			if(bb.tid < 0) continue;

			char buf[1024];
			strcpy(buf, hdr->target_name[bb.tid]);
			bb.chrm = string(buf);

			transcript_set ts1(bb.chrm, 0.9);		// full-length set
			transcript_set ts2(bb.chrm, 0.9);		// non-full-length set

			bundle bd(bb);
			bd.build(1, true);
			bd.print(index++);
			assemble(bd.gr, bd.hs, bb.is_allelic, ts1, ts2);


			bd.build(2, true);
			bd.print(index++);
					
			assemble(bd.gr, bd.hs, bb.is_allelic, ts1, ts2);

			int sdup = assemble_duplicates / 1 + 1;
			int mdup = assemble_duplicates / 2 + 0;

			vector<transcript> gv1 = ts1.get_transcripts(sdup, mdup);
			vector<transcript> gv2 = ts2.get_transcripts(sdup, mdup);

			for(int k = 0; k < gv1.size(); k++)
			{
				if(gv1[k].exons.size() >= 2) gv1[k].coverage /= (1.0 * assemble_duplicates);
			}
			for(int k = 0; k < gv2.size(); k++) 
			{
				if(gv2[k].exons.size() >= 2) gv2[k].coverage /= (1.0 * assemble_duplicates);
			}

			//TODO: modify filters.
			//FIXME: for now, did not use filter.
			
			filter ft1(gv1);
			// ft1.filter_length_coverage();
			// ft1.remove_nested_transcripts();
			if(ft1.trs.size() >= 1) trsts.insert(trsts.end(), ft1.trs.begin(), ft1.trs.end());

			filter ft2(gv2);
			// ft2.filter_length_coverage();
			// ft2.remove_nested_transcripts();
			if(ft2.trs.size() >= 1) non_full_trsts.insert(non_full_trsts.end(), ft2.trs.begin(), ft2.trs.end());
		}
		catch (BundleError e)
		{
			cerr << "Caught bundle error" << endl;
			continue;
		}
	}
	pool.clear();
	return 0;
}

int assembler::assemble(const splice_graph &gr0, const hyper_set &hs0, bool is_allelic, transcript_set &ts1, transcript_set &ts2)
{
	if(DEBUG_MODE_ON)
	{
		for (int i = 0  ;  i < gr0.vinf.size(); i++)
		{
			cout << "gr0 bef scallop first round: " << i << " " << gt_str(gr0.vinf[i].gt) << endl;
		}
	}

	super_graph sg(gr0, hs0);
	sg.build();

	for(int k = 0; k < sg.subs.size(); k++)
	{
		splice_graph gr = sg.subs[k];
		hyper_set &hs = sg.hss[k];

		if(determine_regional_graph(gr) == true) continue;
		if(gr.num_edges() <= 0) continue;
		if(debug_bundle_only) continue; //debug parameter to build bundle only and skip assembly, default: false
		try 
		{
			for(int r = 0; r < assemble_duplicates; r++)
			{
				string gid = "gene." + tostring(index) + "." + tostring(k) + "." + tostring(r);
				
				// TODO: remove
				if(DEBUG_MODE_ON)
				{
					for (int i = 0 ;  i < gr.vinf.size(); i++)
					{
						cout << "bef scallop first round: " << i << " ";
						cout << gt_str(gr.vinf[i].gt) << endl;
					}
				}

				gr.gid = gid;

				// partial decomp of non-AS nodes
				scallop sc(gr, hs, r == 0 ? false : true, true);
				sc.assemble(is_allelic);  
				for(int i = 0; i < sc.trsts.size(); i++)
				{
					ts1.add(sc.trsts[i], 1, 0, TRANSCRIPT_COUNT_ADD_COVERAGE_MIN, TRANSCRIPT_COUNT_ADD_COVERAGE_ADD);
				}
				for(int i = 0; i < sc.non_full_trsts.size(); i++)
				{
					ts2.add(sc.non_full_trsts[i], 1, 0, TRANSCRIPT_COUNT_ADD_COVERAGE_MIN, TRANSCRIPT_COUNT_ADD_COVERAGE_ADD);
				}

				if(verbose >=3) for(auto& i: sc.paths) i.print(index);
				
				// TODO: remove
				if(DEBUG_MODE_ON)
				{
					cout << "print graph aft sc 1-round" << endl;
					sc.gr.print();
					for (int i = 0; i < sc.gr.vinf.size(); i++)
					{
						cout << "aft scallop first round: " << i << " ";
						cout << gt_str(sc.gr.vinf[i].gt) << endl;
					}
				}
				
				// split graph
				// FIXME: eventually should decompose all graphs incld. non-as splice graphs
				if(sc.asnonzeroset.size() <= 0)
				{
					cerr << "did not handle non-AS graphs yet" << endl;
					throw BundleError();
				}
				
				// assemble alleles in seperate splice graphs/ scallops
				splice_graph gr1, gr2;
				hyper_set hs1, hs2;
				scallop* psc1 = nullptr; 
				scallop* psc2 = nullptr;
				phaser ph(sc, &gr1, &hs1, &gr2, &hs2, psc1, psc2);
				
				scallop& sc1 = *psc1;
				scallop& sc2 = *psc2;
				sc1.assemble(is_allelic);  
				sc2.assemble(is_allelic);  
				
				// collect transcripts
				if(verbose >= 2)
				{
					printf("assembly with r = %d, total %lu transcripts:\n", r, sc1.trsts.size());
					for(int i = 0; i < sc1.trsts.size(); i++) sc1.trsts[i].write(cout);
					printf("assembly with r = %d, total %lu transcripts:\n", r, sc2.trsts.size());
					for(int i = 0; i < sc2.trsts.size(); i++) sc2.trsts[i].write(cout);
				}

				//FIXME:TODO: did not filter
				trsts.insert(trsts.end(), sc1.trsts.begin(), sc1.trsts.end());
				trsts.insert(trsts.end(), sc2.trsts.begin(), sc2.trsts.end());
				continue;

				for(int i = 0; i < sc1.trsts.size(); i++)
				{
					ts1.add(sc1.trsts[i], 1, 0, TRANSCRIPT_COUNT_ADD_COVERAGE_MIN, TRANSCRIPT_COUNT_ADD_COVERAGE_ADD);
				}
				for(int i = 0; i < sc1.non_full_trsts.size(); i++)
				{
					ts2.add(sc1.non_full_trsts[i], 1, 0, TRANSCRIPT_COUNT_ADD_COVERAGE_MIN, TRANSCRIPT_COUNT_ADD_COVERAGE_ADD);
				}
				for(int i = 0; i < sc2.trsts.size(); i++)
				{
					ts1.add(sc2.trsts[i], 1, 0, TRANSCRIPT_COUNT_ADD_COVERAGE_MIN, TRANSCRIPT_COUNT_ADD_COVERAGE_ADD);
				}
				for(int i = 0; i < sc2.non_full_trsts.size(); i++)
				{
					ts2.add(sc2.non_full_trsts[i], 1, 0, TRANSCRIPT_COUNT_ADD_COVERAGE_MIN, TRANSCRIPT_COUNT_ADD_COVERAGE_ADD);
				}
			}
		}
		catch (BundleError e)
		{
			cerr << "BundleError in scallop subgraph" << endl;
		}
	}
	return 0;
}

bool assembler::determine_regional_graph(splice_graph &gr)
{
	bool all_regional = true;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		if(gr.get_vertex_info(i).regional == false) all_regional = false;
		if(all_regional == false) break;
	}
	return all_regional;
}

int assembler::assign_RPKM()
{
	double factor = 1e9 / qlen;
	for(int i = 0; i < trsts.size(); i++)
	{
		trsts[i].assign_RPKM(factor);
	}
	return 0;
}

int assembler::write()
{
	ofstream fout((output_file+".gtf").c_str());
	ofstream gvfout((output_file+".gvf").c_str());
	ofstream faout((output_file+".fa").c_str());
	// ofstream asout((output_file+".ASOnly.fa").c_str());
	if(fout.fail()) return 0;
	if(faout.fail()) return 0;
	for(int i = 0; i < trsts.size(); i++)
	{
		transcript &t = trsts[i];
		t.write(fout);
		t.write_gvf(gvfout);
		if(fasta_input != "") t.write_fasta(faout, 60, fai);
		// if(fasta_input != "") t.write_fasta_AS_only(asout, 60, fai);
	}
	fout.close();
	faout.close();
	gvfout.close();

	ofstream fout1(output_file1.c_str());
	if(fout1.fail()) return 0;
	for(int i = 0; i < non_full_trsts.size(); i++)
	{
			transcript &t = non_full_trsts[i];
			t.write(fout1);
	}
    fout1.close();
	return 0;
}