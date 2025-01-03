/*
Part of Scallop Transcript Assembler
(c) 2017 by  Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
See LICENSE for licensing.
*/

#include "hyper_set.h"
#include "config.h"
#include <algorithm>
#include <cstdio>

hyper_set& hyper_set::operator=(const hyper_set &hs)
{
	nodes = hs.nodes;
	edges = hs.edges;
	ecnts = hs.ecnts;
	e2s = hs.e2s;
	edges_to_transform = hs.edges_to_transform;
	return *this;
}

int hyper_set::clear()
{
	nodes.clear();
	edges.clear();
	e2s.clear();
	ecnts.clear();
	edges_to_transform.clear();
	return 0;
}

int hyper_set::add_node_list(const set<int> &s)
{
	return add_node_list(s, 1);
}

int hyper_set::add_node_list(const set<int> &s, int c)
{
	vector<int> v(s.begin(), s.end());
	return add_node_list(v, c);
}

int hyper_set::add_node_list(const vector<int> &s, int c)
{
	vector<int> v = s;
	sort(v.begin(), v.end());
	for(int i = 0; i < v.size(); i++) v[i]++;
	if(nodes.find(v) == nodes.end()) nodes.insert(PVII(v, c));
	else nodes[v] += c;
	return 0;
}

// compatible: hyper_set::build_index()
// NOT compatible: hyper_set::build(), hyper_set::build_edges()
int hyper_set::add_edge_list(const MVII& s)
{
	nodes.clear();
	edges.clear();
	e2s.clear();
	ecnts.clear();
	edges_to_transform.clear();
	for(auto i = s.begin(); i != s.end(); ++i)
	{
		const vector<int>& edge_idx_list = i->first;
		int c = i->second;
		edges_to_transform.push_back(edge_idx_list);
		ecnts.push_back(c);
	}
	assert(edges_to_transform.size() == ecnts.size());
	return 0;
}

/* 
**  transform original-indexed hs to new-indexed hs
**  original i2e ---> (origianl edge_descriptor) ---> x2y ---> (new edge_descriptor) ---> e2i ---> (new edge index)
*/
int hyper_set::transform(const directed_graph* pgr, const VE& i2e_old, const MEE& x2y, const MEI& e2i_new)
{
	assert(nodes.size() == 0);  // transform is only compatible w. add_edge_list, where nodes are never used
	assert(edges.size() == 0);
	assert(edges_to_transform.size() == ecnts.size());
	
	if(edges_to_transform.size() == 0 && DEBUG_MODE_ON && verbose >= 3) cout << "hyper_set is empty when transforming!" << endl;
	if(edges_to_transform.size() == 0 && DEBUG_MODE_ON && verbose >= 3) cerr << "hyper_set is empty when transforming!" << endl;
	/* if (DEBUG_MODE_ON) for(const auto& es: edges_to_transform) {for(int i: es) cout << i2e_old[i] <<" "; cout <<endl;} */
	
	vector<int> ecnts_transformed;	
	for(int i = 0; i < edges_to_transform.size(); i++)
	{
		const vector<int> & vv =  edges_to_transform[i];
		vector<int> ve;
		bool keep_vv = true;

		for(int k : vv)
		{
			if (k == -1)
			{
				ve.push_back(-1);
				continue;
			}

			assert(k >= 0 && k < i2e_old.size());
			auto e_old = i2e_old[k]; 				// index ---> original edge_descriptor
			assert(e_old != null_edge);
			
			auto x2yit = x2y.find(e_old);  			// original edges descriptor ---> new edge descriptor
			assert(x2yit != x2y.end());

			auto e_new = x2yit->second;
			auto e2iit = e2i_new.find(e_new);  		// new edge descriptor ---> new index			

			if(pgr->edge(e_new).second && e2iit != e2i_new.end()) 
			{
				ve.push_back(e2iit->second);
			}
			else
			{
				keep_vv = false;
			}
		}

		if(keep_vv)
		{
			assert(vv.size() == ve.size());
			edges.push_back(ve);
			ecnts_transformed.push_back(ecnts[i]);
		}
	}

	if(edges.size() == 0 && edges_to_transform.size() != 0 && DEBUG_MODE_ON && verbose >= 3) cout << "hyper_set becomes empty after transforming!" << endl;
	if(edges.size() == 0 && edges_to_transform.size() != 0 && DEBUG_MODE_ON && verbose >= 3) cerr << "hyper_set becomes empty after transforming!" << endl;

	ecnts = ecnts_transformed;
	edges_to_transform.clear();

	assert(edges.size() == ecnts.size());
	return 0;
}

int hyper_set::build(directed_graph &gr, MEI& e2i)
{
	build_edges(gr, e2i);
	build_index();
	return 0;
}

int hyper_set::build_edges(directed_graph &gr, MEI& e2i)
{
	assert(edges.size() == 0);
	edges.clear();
	for(MVII::iterator it = nodes.begin(); it != nodes.end(); it++)
	{
		int c = it->second;
		if(c < min_router_count) continue;

		const vector<int> &vv = it->first;
		vector<int> ve;
		bool b = true;
		for(int k = 0; k < vv.size() - 1; k++)
		{
			PEB p = gr.edge(vv[k], vv[k + 1]);
			if(p.second == false) b = false;
			if(p.second == false) ve.push_back(-1);
			else ve.push_back(e2i[p.first]);
		}

		if(b == true && ve.size() >= 2)
		{
			edges.push_back(ve);
			ecnts.push_back(c);
		}
		continue;

		vector<int> v;
		for(int k = 0; k < ve.size(); k++)
		{
			if(ve[k] == -1)
			{
				if(v.size() >= 2)
				{
					edges.push_back(v);
					ecnts.push_back(c);
				}
				v.clear();
			}
			else
			{
				v.push_back(ve[k]);
			}
		}
		if(v.size() >= 2)
		{
			edges.push_back(v);
			ecnts.push_back(c);
		}
	}
	return 0;
}

int hyper_set::build_index()
{
	e2s.clear();
	for(int i = 0; i < edges.size(); i++)
	{
		vector<int> &v = edges[i];
		for(int j = 0; j < v.size(); j++)
		{
			int e = v[j];
			if(e == -1) continue;
			if(e2s.find(e) == e2s.end())
			{
				set<int> s;
				s.insert(i);
				e2s.insert(PISI(e, s));
			}
			else
			{
				e2s[e].insert(i);
			}
		}
	}
	return 0;
}

int hyper_set::update_index()
{
	vector<int> fb1;
	for(MISI::iterator p = e2s.begin(); p != e2s.end(); p++)
	{
		int e = p->first;
		set<int> &ss = p->second;
		vector<int> fb2;
		for(set<int>::iterator it = ss.begin(); it != ss.end(); it++)
		{
			vector<int> &v = edges[*it];
			for(int i = 0; i < v.size(); i++)
			{
				if(v[i] != e) continue;
				bool b1 = false, b2 = false;
				if(i == 0 || v[i - 1] == -1) b1 = true;
				if(i == v.size() - 1 || v[i + 1] == -1) b2 = true;
				if(b1 == true && b2 == true) fb2.push_back(*it);
				break;
			}
		}
		for(int i = 0; i < fb2.size(); i++) ss.erase(fb2[i]);
		if(ss.size() == 0) fb1.push_back(e);
	}
	for(int i = 0; i < fb1.size(); i++) e2s.erase(fb1[i]);
	return 0;
}

set<int> hyper_set::get_intersection(const vector<int> &v)
{
	set<int> ss;
	if(v.size() == 0) return ss;
	assert(v[0] >= 0);
	if(e2s.find(v[0]) == e2s.end()) return ss;
	ss = e2s[v[0]];
	vector<int> vv(ss.size());
	for(int i = 1; i < v.size(); i++)
	{
		assert(v[i] >= 0);
		set<int> s;
		if(e2s.find(v[i]) == e2s.end()) return s;
		s = e2s[v[i]];
		vector<int>::iterator it = set_intersection(ss.begin(), ss.end(), s.begin(), s.end(), vv.begin());
		ss = set<int>(vv.begin(), it);
	}
	return ss;
}

MI hyper_set::get_successors(int e)
{
	MI s;
	if(e2s.find(e) == e2s.end()) return s;
	set<int> &ss = e2s[e];
	for(set<int>::iterator it = ss.begin(); it != ss.end(); it++)
	{
		vector<int> &v = edges[*it];
		int c = ecnts[*it];
		for(int i = 0; i < v.size(); i++)
		{
			if(v[i] != e) continue;
			if(i >= v.size() - 1) continue;
			int k = v[i + 1];
			if(k == -1) continue;
			if(s.find(k) == s.end()) s.insert(PI(k, c));
			else s[k] += c;
		}
	}
	return s;
}

MI hyper_set::get_predecessors(int e)
{
	MI s;
	if(e2s.find(e) == e2s.end()) return s;
	set<int> &ss = e2s[e];
	for(set<int>::iterator it = ss.begin(); it != ss.end(); it++)
	{
		vector<int> &v = edges[*it];
		int c = ecnts[*it];
		for(int i = 0; i < v.size(); i++)
		{
			if(v[i] != e) continue;
			if(i == 0) continue;
			int k = v[i - 1];
			if(k == -1) continue;
			if(s.find(k) == s.end()) s.insert(PI(k, c));
			else s[k] += c;
		}
	}
	return s;
}

MPII hyper_set::get_routes(int x, directed_graph &gr, MEI &e2i)
{
	MPII mpi;
	edge_iterator it1, it2;
	PEEI pei;
	vector<PI> v;
	for(pei = gr.in_edges(x), it1 = pei.first, it2 = pei.second; it1 != it2; it1++)
	{
		assert(e2i.find(*it1) != e2i.end());
		int e = e2i[*it1];
		MI s = get_successors(e);
		for(MI::iterator it = s.begin(); it != s.end(); it++)
		{
			PI p(e, it->first);
			mpi.insert(PPII(p, it->second));
		}
	}
	return mpi;
}

/*
int hyper_set::get_routes(int x, directed_graph &gr, MEI &e2i, MPII &mpi)
{
	edge_iterator it1, it2;
	mpi.clear();
	int total = 0;
	for(tie(it1, it2) = gr.in_edges(x); it1 != it2; it1++)
	{
		assert(e2i.find(*it1) != e2i.end());
		int e = e2i[*it1];

		if(e2s.find(e) == e2s.end()) continue;
		set<int> &ss = e2s[e];
		for(set<int>::iterator it = ss.begin(); it != ss.end(); it++)
		{
			int k = *it;
			assert(k >= 0 && k < edges.size());
			assert(k >= 0 && k < ecnts.size());
			vector<int> &v = edges[k];
			int cnt = ecnts[k];
			for(int i = 0; i < v.size(); i++)
			{
				if(v[i] != e) continue;
				if(i == v.size() - 1) continue;
				if(v[i + 1] == -1) continue;
				PI p(e, v[i + 1]);
				total += cnt;
				if(mpi.find(p) != mpi.end()) mpi[p] += cnt;
				else mpi.insert(PPII(p, cnt));
			}
		}
	}
	return total;
}
*/

int hyper_set::replace(int x, int e)
{
	vector<int> v;
	v.push_back(x);
	replace(v, e);
	return 0;
}

int hyper_set::replace(int x, int y, int e)
{
	vector<int> v;
	v.push_back(x);
	v.push_back(y);
	replace(v, e);
	return 0;
}

int hyper_set::replace(const vector<int> &v, int e)
{
	if(v.size() == 0) return 0;
	set<int> s = get_intersection(v);
	
	vector<int> fb;
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		vector<int> bv = consecutive_subset(vv, v);

		if(bv.size() <= 0) continue;
		assert(bv.size() == 1);

		int b = bv[0];
		vv[b] = e;
		
		/* get rid of testing useful
		bool b1 = useful(vv, 0, b);
		bool b2 = useful(vv, b + v.size() - 1, vv.size() - 1);

		if(b1 == false && b2 == false)
		{
			fb.push_back(k);
			continue;
		}
		*/

		vv.erase(vv.begin() + b + 1, vv.begin() + b + v.size());

		fb.push_back(k);

		if(e2s.find(e) == e2s.end())
		{
			set<int> ss;
			ss.insert(k);
			e2s.insert(PISI(e, ss));
		}
		else
		{
			e2s[e].insert(k);
		}
	}

	for(int i = 0; i < v.size(); i++)
	{
		int u = v[i];
		if(e2s.find(u) == e2s.end()) continue;
		for(int k = 0; k < fb.size(); k++) e2s[u].erase(fb[k]);
		if(e2s[u].size() == 0) e2s.erase(u);
	}
	return 0;
}

int hyper_set::remove(const set<int> &s)
{
	return remove(vector<int>(s.begin(), s.end()));
}

int hyper_set::remove(const vector<int> &v)
{
	for(int i = 0; i < v.size(); i++) remove(v[i]);
	return 0;
}

int hyper_set::remove(int e)
{
	if(e2s.find(e) == e2s.end()) return 0;
	set<int> &s = e2s[e];
	vector<int> fb;
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);

		for(int i = 0; i < vv.size(); i++)
		{
			if(vv[i] != e) continue;

			vv[i] = -1;
			fb.push_back(k);

			/*
			bool b1 = useful(vv, 0, i - 1);
			bool b2 = useful(vv, i + 1, vv.size() - 1);
			if(b1 == false && b2 == false) fb.push_back(k);
			 
			break;
			*/
		}
	}

	for(int i = 0; i < fb.size(); i++) s.erase(fb[i]);
	e2s.erase(e);
	return 0;
}

int hyper_set::remove_pair(int x, int y)
{
	insert_between(x, y, -1);
	return 0;

	if(e2s.find(x) == e2s.end()) return 0;
	set<int> &s = e2s[x];
	vector<int> fb;
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);

		for(int i = 0; i < vv.size(); i++)
		{
			if(i == vv.size() - 1) continue;
			if(vv[i] != x) continue;
			if(vv[i + 1] != y) continue;

			bool b1 = useful(vv, 0, i);
			bool b2 = (b1 == true) ? true : useful(vv, i + 1, vv.size() - 1);

			if(b1 == false && b2 == false) fb.push_back(k);
			else vv.insert(vv.begin() + i + 1, -1);

			break;
		}
	}

	for(int i = 0; i < fb.size(); i++) s.erase(fb[i]);
	if(s.size() == 0) e2s.erase(x);

	return 0;
}

bool hyper_set::useful(const vector<int> &v, int k1, int k2)
{
	for(int i = k1; i < k2; i++)
	{
		if(v[i] >= 0 && v[i + 1] >= 0) return true;
	}
	return false;
}

int hyper_set::insert_between(int x, int y, int e)
{
	if(e2s.find(x) == e2s.end()) return 0;
	set<int> s = e2s[x];
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);

		for(int i = 0; i < vv.size(); i++)
		{
			if(i == vv.size() - 1) continue;
			if(vv[i] != x) continue;
			if(vv[i + 1] != y) continue;
			vv.insert(vv.begin() + i + 1, e);
			
			if(e == -1) continue;

			if(e2s.find(e) == e2s.end())
			{
				set<int> ss;
				ss.insert(k);
				e2s.insert(PISI(e, ss));
			}
			else
			{
				e2s[e].insert(k);
			}

			//printf("line %d: insert %d between (%d, %d) = (%d, %d, %d)\n", k, e, x, y, vv[i], vv[i + 1], vv[i + 2]);

			// break;
		}
	}
	return 0;
}

// bool hyper_set::extend(int e)
// {
// 	return (left_extend(e) || right_extend(e));
// }

bool hyper_set::left_extend(int e)
{
	if(e2s.find(e) == e2s.end()) return false;
	set<int> s = e2s[e];
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);

		for(int i = 1; i < vv.size(); i++)
		{
			if(vv[i] == e && vv[i - 1] != -1) return true; 
		}
	}
	return false;
}

bool hyper_set::right_extend(int e)
{
	if(e2s.find(e) == e2s.end()) return false;
	set<int> s = e2s[e];
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);

		for(int i = 0; i < vv.size() - 1; i++)
		{
			if(vv[i] == e && vv[i + 1] != -1) return true; 
		}
	}
	return false;
}

bool hyper_set::left_extend(const vector<int> &s)
{
	for(int i = 0; i < s.size(); i++)
	{
		if(left_extend(s[i]) == true) return true;
	}
	return false;
}

bool hyper_set::right_extend(const vector<int> &s)
{
	for(int i = 0; i < s.size(); i++)
	{
		if(right_extend(s[i]) == true) return true;
	}
	return false;
}

bool hyper_set::left_dominate(int e)
{
	// for each appearance of e
	// if right is not empty then left is also not empty
	if(e2s.find(e) == e2s.end()) return true;

	set<PI> x1;
	set<PI> x2;
	set<int> s = e2s[e];
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);

		for(int i = 0; i < vv.size() - 1; i++)
		{
			if(vv[i] != e) continue;
			if(vv[i + 1] == -1) continue;

			if(i == 0 || vv[i - 1] == -1)
			{
				if(i + 2 < vv.size()) x1.insert(PI(vv[i + 1], vv[i + 2]));
				else x1.insert(PI(vv[i + 1], -1));
			}
			else
			{
				x2.insert(PI(vv[i + 1], -1));
				if(i + 2 < vv.size()) x2.insert(PI(vv[i + 1], vv[i + 2]));
			}
		}
	}

	for(set<PI>::iterator it = x1.begin(); it != x1.end(); it++)
	{
		PI p = (*it);
		if(x2.find(p) == x2.end()) return false;
	}
	return true;
}

bool hyper_set::right_dominate(int e)
{
	// for each appearance of e
	// if left is not empty then right is also not empty
	if(e2s.find(e) == e2s.end()) return true;
	set<PI> x1;
	set<PI> x2;
	set<int> s = e2s[e];
	for(set<int>::iterator it = s.begin(); it != s.end(); it++)
	{
		int k = (*it);
		vector<int> &vv = edges[k];
		assert(vv.size() >= 1);
		for(int i = 1; i < vv.size(); i++)
		{
			if(vv[i] != e) continue;
			if(vv[i - 1] == -1) continue;

			if(i == vv.size() - 1 || vv[i + 1] == -1)
			{
				if(i - 2 >= 0) x1.insert(PI(vv[i - 1], vv[i - 2]));
				else x1.insert(PI(vv[i - 1], -1));
			}
			else
			{
				x2.insert(PI(vv[i - 1], -1));
				if(i - 2 >= 0) x2.insert(PI(vv[i - 1], vv[i - 2]));
			}
		}
	}

	for(set<PI>::iterator it = x1.begin(); it != x1.end(); it++)
	{
		PI p = (*it);
		if(x2.find(p) == x2.end()) return false;
	}

	return true;
}

int hyper_set::print()
{
	for(MVII::iterator it = nodes.begin(); it != nodes.end(); it++)
	{
		const vector<int> &v = it->first;
		int c = it->second;
		printf("hyper-edge (nodes), counts = %d, list = ( ", c); 
		printv(v);
		printf(")\n");
	}

	for(int i = 0; i < edges.size(); i++)
	{
		printf("hyper-edge (edges) %d: ( ", i);
		printv(edges[i]);
		printf(")\n");
	}

	for(int i = 0; i < edges_to_transform.size(); i++)
	{
		printf("hyper-edge (edges_to_transform) %d: ( ", i);
		printv(edges[i]);
		printf(")\n");
	}

	return 0;
}
