/*
Part of Scallop Transcript Assembler
(c) 2017 by Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
Part of Altai
(c) 2021 by Xiaofei Carl Zang, Mingfu Shao, and The Pennsylvania State University.
See LICENSE for licensing.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <iostream>
#include <ctime>
#include <cassert>
#include <sstream>
#include <stdlib.h>

#include "config.h"
#include "previewer.h"
#include "assembler.h"

using namespace std;

int main(int argc, const char **argv)
{

	srand(time(0));

	if(argc == 1)
	{
		print_copyright();
		print_caution_message();
		print_help();
		printf("\n");
		print_logo();
		return 0;
	}

	parse_arguments(argc, argv);

	
		
	if(verbose >= 1)
	{
		print_copyright();
		print_caution_message();
		printf("\n");
		print_command_line(argc, argv);
		printf("\n");
		if (verbose >= 3) print_parameters();
	}
	
	previewer pv;
	pv.preview();

	if(preview_only == true) return 0;

	assembler asmb;
	asmb.assemble();

	printf("Altai is completed.\n");

	return 0;
}
