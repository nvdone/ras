//NVD command line parsing library
//Copyright © 2019, Nikolay Dudkin

//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU Lesser General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//GNU Lesser General Public License for more details.
//You should have received a copy of the GNU Lesser General Public License
//along with this program.If not, see<https://www.gnu.org/licenses/>.

#ifndef CMDLINE_HPP
#define CMDLINE_HPP

#include <windows.h>
#include <wctype.h>

class Param
{
	private:

		wchar_t *name;
		wchar_t *name_l;
		wchar_t *value;
		wchar_t *value_l;

		int isoption;

		wchar_t *getToken(const wchar_t **str, int isName);

	public:

		Param *Root;
		Param *Leaf;

		Param(Param *root);
		~Param();

		const wchar_t *Parse(const wchar_t *str);

		int IsOption();
		int NameIs(const wchar_t *candidate, int lc);

		wchar_t *GetName(int lc);
		wchar_t *GetValue(int lc);
};

class CmdLine
{
	private:

		Param *root;
		Param *leaf;

		int nParams;
		int nOptions;
		int nCommands;

	public:

		CmdLine(const wchar_t *cmdline);
		~CmdLine();

		int HasParam(const wchar_t *name, int lc);

		Param *GetParam(int n);
		Param *GetOption(int n);
		wchar_t *GetOptionValue(const wchar_t *name, int lc, int lcv);
		Param *GetCommand(int n);

		int CountParams();
		int CountOptions();
		int CountCommands();
};

#endif