/*
Part of Scallop Transcript Assembler
(c) 2017 by  Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
See LICENSE for licensing.
*/

#ifndef __EQUATION_H__
#define __EQUATION_H__

#include <vector>

using namespace std;

class equation
{
public:
	equation();
	equation(double);
	equation(const vector<int> &, const vector<int> &);
	equation(const vector<int> &, const vector<int> &, double);

public:
	int print(int index) const;
	int clear();

public:
	vector<int> s;		// subs
	vector<int> t;		// subt
	double e;			// erro

	int f;				// 3: resolve vertex 2: fully, 1: partly, 0: none
	int a;				// # adjacent merges
	int d;				// # distant merges
	int w;				// weight
};

#endif
