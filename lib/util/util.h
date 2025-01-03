/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <map>
#include <set>
#include <sstream>
#include <cassert>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <exception>
#include "src/as_pos32.hpp"

using namespace std;

// macros: using int64_t for two int32_t
#define pack(x, y) (int64_t)((((int64_t)(x)) << 32) | ((int64_t)(y)))
// #define high32(x) (int32_t)((x) >> 32)
// #define low32(x) (int32_t)(((x) << 32) >> 32)


// definitions
typedef map<as_pos32, as_pos32> MI32;
typedef pair<as_pos32, as_pos32> PI32;
typedef map<as_pos32, int> MPI;
typedef pair<as_pos32, int> PPI;
typedef map<int, int> MI;
typedef pair<int, int> PI;

// common small functions
template<typename T>
string tostring(T t)
{
	ostringstream s;
	s << t;
	return s.str();
}

template<typename T>
T compute_overlap(const pair<T, T> &x, const pair<T, T> &y)
{
	assert(x.first <= x.second);
	assert(y.first <= y.second);
	if(x.first > y.first) return compute_overlap(y, x);
	assert(x.first <= y.first);
	if(y.first >= x.second) return x.second - y.first;
	if(x.second <= y.second) return x.second - y.first;
	else return y.second - y.first;
}

template<typename T>
int reverse(vector<T> &x)
{
	if(x.size() == 0) return 0;
	int i = 0;
	int j = x.size() - 1;
	while(i < j)
	{
		T t = x[i];
		x[i] = x[j];
		x[j] = t;
		i++;
		j--;
	}
	return 0;
}

template<typename T>
int max_element(const vector<T> &x)
{
	if(x.size() == 0) return -1;
	int k = 0;
	for(int i = 1; i < x.size(); i++)
	{
		if(x[i] <= x[k]) continue;
		k = i;
	}
	return k;
}

template<typename T>
int min_element(const vector<T> &x)
{
	if(x.size() == 0) return -1;
	int k = 0;
	for(int i = 1; i < x.size(); i++)
	{
		if(x[i] >= x[k]) continue;
		k = i;
	}
	return k;
}

template<typename T>
int prints(const set<T> &x)
{
	for(typename set<T>::const_iterator it = x.begin(); it != x.end(); it++)
	{
		cout<< *it <<" ";
	}
	return 0;
}

template<typename T>
int printv(const vector<T> &x)
{
	for(int i = 0; i < x.size(); i++)
	{
		cout<< x[i] <<" ";
	}
	return 0;
}

template<typename T>
int compute_mean_dev(const vector<T> &v, int si, int ti, double &ave, double &dev)
{
	ave = -1;
	dev = -1;
	if(si >= ti) return 0;

	assert(si >= 0 && si < v.size());
	assert(ti > 0 && ti <= v.size());

	T sum = 0;
	for(int i = si; i < ti; i++)
	{
		sum += v[i];
	}

	ave = sum * 1.0 / (ti - si);

	double var;
	for(int i = si ; i < ti; i++)
	{
		var += (v[i] - ave) * (v[i] - ave);
	}

	dev = sqrt(var / (ti - si));
	return 0;
}

template<typename T>
vector<int> consecutive_subset(const vector<T> &ref, const vector<T> &x)
{
	vector<int> v;
	if(x.size() == 0) return v;
	if(ref.size() == 0) return v;
	if(x.size() > ref.size()) return v;
	for(int i = 0; i <= ref.size() - x.size(); i++)
	{
		if(ref[i] != x[0]) continue;
		int k = i;
		bool b = true;
		for(int j = 0; j < x.size(); j++)
		{
			if(x[j] != ref[j + k]) b = false;
			if(b == false) break;
		}
		if(b == false) continue;
		v.push_back(k);
	}
	return v;
}

template<typename K, typename V>
vector<K> get_keys(const map<K, V> &m)
{
	vector<K> v;
    typedef typename std::map<K,V>::const_iterator MIT;
	for(MIT it = m.begin(); it != m.end(); it++)
	{
		v.push_back(it->first);
	}
	return v;
}

string toupperstring(const string s);
vector<string> split(const string s, const string sep);
size_t string_hash(const std::string& str);
// size_t vector_hash(const vector<int32_t> &str);
size_t vector_hash(const vector<as_pos32> &str);
int reverse_complement_DNA(string &rc, const string s);

class BundleError					// TODO: tmp DEBUG helper
{
	
};

#endif
