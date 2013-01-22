/*
 *  Copyright 2001-2011 Adrian Thurston <thurston@complang.org>
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

#include "ragel.h"
#include "dot.h"
#include "gendata.h"
#include "inputdata.h"
#include "rlparse.h"
#include "rlscan.h"

using std::istream;
using std::ifstream;
using std::ostream;
using std::ios;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

void GraphvizDotGen::key( Key key )
{
	if ( displayPrintables && key.isPrintable() ) {
		// Output values as characters, ensuring we escape the quote (") character
		char cVal = (char) key.getVal();
		switch ( cVal ) {
			case '"': case '\\':
				out << "'\\" << cVal << "'";
				break;
			case '\a':
				out << "'\\\\a'";
				break;
			case '\b':
				out << "'\\\\b'";
				break;
			case '\t':
				out << "'\\\\t'";
				break;
			case '\n':
				out << "'\\\\n'";
				break;
			case '\v':
				out << "'\\\\v'";
				break;
			case '\f':
				out << "'\\\\f'";
				break;
			case '\r':
				out << "'\\\\r'";
				break;
			case ' ':
				out << "SP";
				break;
			default:	
				out << "'" << cVal << "'";
				break;
		}
	}
	else {
		if ( keyOps->isSigned )
			out << key.getVal();
		else
			out << (unsigned long) key.getVal();
	}
}


void GraphvizDotGen::onChar( Key lowKey, Key highKey, CondSpace *condSpace, long condVals )
{
	/* Output the key. Possibly a range. */
	key( lowKey );
	if ( keyOps->ne( highKey, lowKey ) ) {
		out << "..";
		key( highKey );
	}

	if ( condSpace != 0 ) {
		out << "(";
		for ( CondSet::Iter csi = condSpace->condSet; csi.lte(); csi++ ) {
			bool set = condVals & (1 << csi.pos());
			if ( !set )
				out << "!";
			(*csi)->actionName( out );
			if ( !csi.last() )
				out << ", ";
		}
		out << ")";
	}
}


void GraphvizDotGen::transAction( StateAp *fromState, CondAp *trans )
{
	int n = 0;
	ActionTable *actionTables[3] = { 0, 0, 0 };

	if ( fromState->fromStateActionTable.length() != 0 )
		actionTables[n++] = &fromState->fromStateActionTable;
	if ( trans->actionTable.length() != 0 )
		actionTables[n++] = &trans->actionTable;
	if ( trans->toState != 0 && trans->toState->toStateActionTable.length() != 0 )
		actionTables[n++] = &trans->toState->toStateActionTable;

	if ( n > 0 )
		out << " / ";
	
	/* Loop the existing actions and write out what's there. */
	for ( int a = 0; a < n; a++ ) {
		for ( ActionTable::Iter actIt = actionTables[a]->first(); actIt.lte(); actIt++ ) {
			Action *action = actIt->value;
			action->actionName( out );
			if ( a < n-1 || !actIt.last() )
				out << ", ";
		}
	}
}

void GraphvizDotGen::action( ActionTable *actionTable )
{
	/* The action. */
	out << " / ";
	for ( ActionTable::Iter actIt = actionTable->first(); actIt.lte(); actIt++ ) {
		Action *action = actIt->value;
		action->actionName( out );
		if ( !actIt.last() )
			out << ", ";
	}
}

void GraphvizDotGen::transList( StateAp *state )
{
	/* Build the set of unique transitions out of this state. */
	RedTransSet stTransSet;
	for ( TransList::Iter tel = state->outList; tel.lte(); tel++ ) {
		for ( CondList::Iter ctel = tel->condList; ctel.lte(); ctel++ ) {
			/* Write out the from and to states. */
			out << "\t" << state->alg.stateNum << " -> ";

			if ( ctel->toState == 0 )
				out << "err_" << state->alg.stateNum;
			else
				out << ctel->toState->alg.stateNum;

			/* Begin the label. */
			out << " [ label = \""; 
			onChar( tel->lowKey, tel->highKey, tel->condSpace, ctel->key.getVal() );

			/* Write the action and close the transition. */
			transAction( state, ctel );
			out << "\" ];\n";
		}
	}
}

bool GraphvizDotGen::makeNameInst( std::string &res, NameInst *nameInst )
{
	bool written = false;
	if ( nameInst->parent != 0 )
		written = makeNameInst( res, nameInst->parent );
	
	if ( nameInst->name != 0 ) {
		if ( written )
			res += '_';
		res += nameInst->name;
		written = true;
	}

	return written;
}

void GraphvizDotGen::write( )
{
	out << 
		"digraph " << pd->sectionName << " {\n"
		"	rankdir=LR;\n";
	
	/* Define the psuedo states. Transitions will be done after the states
	 * have been defined as either final or not final. */
	out << "	node [ shape = point ];\n";

	if ( fsm->startState != 0 )
		out << "	ENTRY;\n";

	/* Psuedo states for entry points in the entry map. */
	for ( EntryMap::Iter en = fsm->entryPoints; en.lte(); en++ ) {
		StateAp *state = en->value;
		out << "	en_" << state->alg.stateNum << ";\n";
	}

	/* Psuedo states for final states with eof actions. */
	for ( StateList::Iter st = fsm->stateList; st.lte(); st++ ) {
		//if ( st->eofTrans != 0 && st->eofTrans->action != 0 )
		//	out << "	eof_" << st->id << ";\n";
		if ( st->eofActionTable.length() > 0 )
			out << "	eof_" << st->alg.stateNum << ";\n";
	}

	out << "	node [ shape = circle, height = 0.2 ];\n";

	/* Psuedo states for states whose default actions go to error. */
	for ( StateList::Iter st = fsm->stateList; st.lte(); st++ ) {
		bool needsErr = false;
		for ( TransList::Iter tel = st->outList; tel.lte(); tel++ ) {
			for ( CondList::Iter ctel = tel->condList; ctel.lte(); ctel++ ) {
				if ( ctel->toState == 0 ) {
					needsErr = true;
					break;
				}
			}
		}

		if ( needsErr )
			out << "	err_" << st->alg.stateNum << " [ label=\"\"];\n";
	}

	/* Attributes common to all nodes, plus double circle for final states. */
	out << "	node [ fixedsize = true, height = 0.65, shape = doublecircle ];\n";

	/* List Final states. */
	for ( StateList::Iter st = fsm->stateList; st.lte(); st++ ) {
		if ( st->isFinState() )
			out << "	" << st->alg.stateNum << ";\n";
	}

	/* List transitions. */
	out << "	node [ shape = circle ];\n";

	/* Walk the states. */
	for ( StateList::Iter st = fsm->stateList; st.lte(); st++ )
		transList( st );

	/* Transitions into the start state. */
	if ( fsm->startState != 0 ) 
		out << "	ENTRY -> " << fsm->startState->alg.stateNum << " [ label = \"IN\" ];\n";

	for ( EntryMap::Iter en = fsm->entryPoints; en.lte(); en++ ) {
		NameInst *nameInst = pd->nameIndex[en->key];
		std::string name;
		makeNameInst( name, nameInst );
		StateAp *state = en->value;
		out << "	en_" << state->alg.stateNum << 
				" -> " << state->alg.stateNum <<
				" [ label = \"" << name << "\" ];\n";
	}

	/* Out action transitions. */
	for ( StateList::Iter st = fsm->stateList; st.lte(); st++ ) {
		if ( st->eofActionTable.length() != 0 ) {
			out << "	" << st->alg.stateNum << " -> eof_" << 
					st->alg.stateNum << " [ label = \"EOF"; 
			action( &st->eofActionTable );
			out << "\" ];\n";
		}
	}

	out <<
		"}\n";
}

void InputData::writeDot( ostream &out )
{
	ParseData *pd = dotGenParser->pd;
	FsmAp *graph = pd->sectionGraph;

	CodeGenArgs args( *this, inputFileName, pd->sectionName, pd, graph, out );

	GraphvizDotGen dotGen( args );

	dotGen.write();
}

