/*
 *  Copyright 2001-2006 Adrian Thurston <thurston@complang.org>
 *            2004 Erich Ocean <eric.ocean@ampede.com>
 *            2005 Alan West <alan@alanz.com>
 */

/*  This file is part of Ragel.
 *
 *  Ragel is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Ragel is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#ifndef _DIPGOTO_H
#define _DIPGOTO_H

#include <iostream>
#include "goto.h"

/* Forwards. */
struct CodeGenData;

namespace D {

/*
 * class FGotoCodeGen
 */
class IpGotoCodeGen : public GotoCodeGen
{
public:
	IpGotoCodeGen( const CodeGenArgs &args ) 
			: FsmCodeGen(args), GotoCodeGen(args) {}

	std::ostream &EXIT_STATES();
	std::ostream &TRANS_GOTO( RedTransAp *trans, int level );
	std::ostream &FINISH_CASES();
	std::ostream &AGAIN_CASES();

	void GOTO( ostream &ret, int gotoDest, bool inFinish );
	void CALL( ostream &ret, int callDest, int targState, bool inFinish );
	void NEXT( ostream &ret, int nextDest, bool inFinish );
	void GOTO_EXPR( ostream &ret, GenInlineItem *ilItem, bool inFinish );
	void NEXT_EXPR( ostream &ret, GenInlineItem *ilItem, bool inFinish );
	void CALL_EXPR( ostream &ret, GenInlineItem *ilItem, int targState, bool inFinish );
	void RET( ostream &ret, bool inFinish );
	void CURS( ostream &ret, bool inFinish );
	void TARGS( ostream &ret, bool inFinish, int targState );
	void BREAK( ostream &ret, int targState, bool csForced );

	virtual void writeData();
	virtual void writeExec();

protected:
	bool useAgainLabel();

	/* Called from GotoCodeGen::STATE_GOTOS just before writing the gotos for
	 * each state. */
	bool IN_TRANS_ACTIONS( RedStateAp *state );
	void GOTO_HEADER( RedStateAp *state );
	void STATE_GOTO_ERROR();

	/* Set up labelNeeded flag for each state. */
	void setLabelsNeeded( GenInlineList *inlineList );
	void setLabelsNeeded();
};


/*
 * class CIpGotoCodeGen
 */
struct CIpGotoCodeGen
	: public IpGotoCodeGen, public CCodeGen
{
	CIpGotoCodeGen( const CodeGenArgs &args) : 
		FsmCodeGen(args), IpGotoCodeGen(args), CCodeGen(args) {}
};

/*
 * class DIpGotoCodeGen
 */
struct DIpGotoCodeGen
	: public IpGotoCodeGen, public DCodeGen
{
	DIpGotoCodeGen( const CodeGenArgs &args) : 
		FsmCodeGen(args), IpGotoCodeGen(args), DCodeGen(args) {}
};

/*
 * class D2IpGotoCodeGen
 */
struct D2IpGotoCodeGen
	: public IpGotoCodeGen, public D2CodeGen
{
	D2IpGotoCodeGen( const CodeGenArgs &args) : 
		FsmCodeGen(args), IpGotoCodeGen(args), D2CodeGen(args) {}
};

}

#endif
