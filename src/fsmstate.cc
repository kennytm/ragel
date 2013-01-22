/*
 *  Copyright 2002 Adrian Thurston <thurston@complang.org>
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

#include "fsmgraph.h"

#include <string.h>
#include <assert.h>
#include <iostream>

/* Construct a mark index for a specified number of states. Must new up
 * an array that is states^2 in size. */
MarkIndex::MarkIndex( int states ) : numStates(states)
{
	/* Total pairs is states^2. Actually only use half of these, but we allocate
	 * them all to make indexing into the array easier. */
	int total = states * states;

	/* New up chars so that individual DListEl constructors are
	 * not called. Zero out the mem manually. */
	array = new bool[total];
	memset( array, 0, sizeof(bool) * total );
}

/* Free the array used to store state pairs. */
MarkIndex::~MarkIndex()
{
	delete[] array;
}

/* Mark a pair of states. States are specified by their number. The
 * marked states are moved from the unmarked list to the marked list. */
void MarkIndex::markPair(int state1, int state2)
{
	int pos = ( state1 >= state2 ) ?
		( state1 * numStates ) + state2 :
		( state2 * numStates ) + state1;

	array[pos] = true;
}

/* Returns true if the pair of states are marked. Returns false otherwise.
 * Ordering of states given does not matter. */
bool MarkIndex::isPairMarked(int state1, int state2)
{
	int pos = ( state1 >= state2 ) ?
		( state1 * numStates ) + state2 :
		( state2 * numStates ) + state1;

	return array[pos];
}

/* Create a new fsm state. State has not out transitions or in transitions, not
 * out out transition data and not number. */
StateAp::StateAp()
:
	/* No out or in transitions. */
	outList(),
	inList(),

	/* No EOF target. */
	eofTarget(0),

	/* No entry points, or epsilon trans. */
	entryIds(),
	epsilonTrans(),

	/* No transitions in from other states. */
	foreignInTrans(0),

	/* Only used during merging. Normally null. */
	stateDictEl(0),
	eptVect(0),

	/* No state identification bits. */
	stateBits(0),

	/* No Priority data. */
	outPriorTable(),

	/* No Action data. */
	toStateActionTable(),
	fromStateActionTable(),
	outActionTable(),
	outCondSet(),
	errActionTable(),
	eofActionTable()
{
}

/* Copy everything except actual the transitions. That is left up to the
 * FsmAp copy constructor. */
StateAp::StateAp(const StateAp &other)
:
	/* All lists are cleared. They will be filled in when the
	 * individual transitions are duplicated and attached. */
	outList(),
	inList(),

	/* Set this using the original state's eofTarget. It will get mapped back
	 * to the new machine in the Fsm copy constructor. */
	eofTarget(other.eofTarget),

	/* Duplicate the entry id set and epsilon transitions. These
	 * are sets of integers and as such need no fixing. */
	entryIds(other.entryIds),
	epsilonTrans(other.epsilonTrans),

	/* No transitions in from other states. */
	foreignInTrans(0),

	/* This is only used during merging. Normally null. */
	stateDictEl(0),
	eptVect(0),

	/* Fsm state data. */
	stateBits(other.stateBits),

	/* Copy in priority data. */
	outPriorTable(other.outPriorTable),

	/* Copy in action data. */
	toStateActionTable(other.toStateActionTable),
	fromStateActionTable(other.fromStateActionTable),
	outActionTable(other.outActionTable),
	outCondSet(other.outCondSet),
	errActionTable(other.errActionTable),
	eofActionTable(other.eofActionTable)
{
	/* Duplicate all the transitions. */
	for ( TransList::Iter trans = other.outList; trans.lte(); trans++ ) {
		/* Duplicate and store the orginal target in the transition. This will
		 * be corrected once all the states have been created. */
		TransAp *newTrans = new TransAp( *trans );

		for ( CondList::Iter cti = trans->condList; cti.lte(); cti++ ) {
			CondAp *newCondTrans = new CondAp( *cti, newTrans );
			newCondTrans->key = cti->key;

			newTrans->condList.append( newCondTrans );

			assert( cti->lmActionTable.length() == 0 );

			newCondTrans->toState = cti->toState;
		}

		outList.append( newTrans );
	}
}

/* If there is a state dict element, then delete it. Everything else is left
 * up to the FsmGraph destructor. */
StateAp::~StateAp()
{
	if ( stateDictEl != 0 )
		delete stateDictEl;
}

/* Compare two states using pointers to the states. With the approximate
 * compare, the idea is that if the compare finds them the same, they can
 * immediately be merged. */
int ApproxCompare::compare( const StateAp *state1, const StateAp *state2 )
{
	int compareRes;

	/* Test final state status. */
	if ( (state1->stateBits & STB_ISFINAL) && !(state2->stateBits & STB_ISFINAL) )
		return -1;
	else if ( !(state1->stateBits & STB_ISFINAL) && (state2->stateBits & STB_ISFINAL) )
		return 1;
	
	/* Test epsilon transition sets. */
	compareRes = CmpEpsilonTrans::compare( state1->epsilonTrans, 
			state2->epsilonTrans );
	if ( compareRes != 0 )
		return compareRes;
	
	/* Compare the out transitions. */
	compareRes = FsmAp::compareStateData( state1, state2 );
	if ( compareRes != 0 )
		return compareRes;

	/* Use a pair iterator to get the transition pairs. */
	RangePairIter<TransAp> outPair( ctx, state1->outList.head, state2->outList.head );
	for ( ; !outPair.end(); outPair++ ) {
		switch ( outPair.userState ) {

		case RangePairIter<TransAp>::RangeInS1:
			compareRes = FsmAp::compareFullPtr( outPair.s1Tel.trans, 0 );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::RangeInS2:
			compareRes = FsmAp::compareFullPtr( 0, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::RangeOverlap:
			compareRes = FsmAp::compareFullPtr( 
					outPair.s1Tel.trans, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::BreakS1:
		case RangePairIter<TransAp>::BreakS2:
			break;
		}
	}

	/* Check EOF targets. */
	if ( state1->eofTarget < state2->eofTarget )
		return -1;
	else if ( state1->eofTarget > state2->eofTarget )
		return 1;

	/* Got through the entire state comparison, deem them equal. */
	return 0;
}

/* Compare class used in the initial partition. */
int InitPartitionCompare::compare( const StateAp *state1 , const StateAp *state2 )
{
	int compareRes;

	/* Test final state status. */
	if ( (state1->stateBits & STB_ISFINAL) && !(state2->stateBits & STB_ISFINAL) )
		return -1;
	else if ( !(state1->stateBits & STB_ISFINAL) && (state2->stateBits & STB_ISFINAL) )
		return 1;

	/* Test epsilon transition sets. */
	compareRes = CmpEpsilonTrans::compare( state1->epsilonTrans, 
			state2->epsilonTrans );
	if ( compareRes != 0 )
		return compareRes;

	/* Compare the out transitions. */
	compareRes = FsmAp::compareStateData( state1, state2 );
	if ( compareRes != 0 )
		return compareRes;

	/* Use a pair iterator to test the transition pairs. */
	RangePairIter<TransAp> outPair( ctx, state1->outList.head, state2->outList.head );
	for ( ; !outPair.end(); outPair++ ) {
		switch ( outPair.userState ) {

		case RangePairIter<TransAp>::RangeInS1:
			compareRes = FsmAp::compareTransDataPtr( outPair.s1Tel.trans, 0 );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::RangeInS2:
			compareRes = FsmAp::compareTransDataPtr( 0, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::RangeOverlap:
			compareRes = FsmAp::compareTransDataPtr( 
					outPair.s1Tel.trans, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::BreakS1:
		case RangePairIter<TransAp>::BreakS2:
			break;
		}
	}

	return 0;
}

/* Compare class for the sort that does the partitioning. */
int PartitionCompare::compare( const StateAp *state1, const StateAp *state2 )
{
	int compareRes;

	/* Use a pair iterator to get the transition pairs. */
	RangePairIter<TransAp> outPair( ctx, state1->outList.head, state2->outList.head );
	for ( ; !outPair.end(); outPair++ ) {
		switch ( outPair.userState ) {

		case RangePairIter<TransAp>::RangeInS1:
			compareRes = FsmAp::compareTransPartPtr( outPair.s1Tel.trans, 0 );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::RangeInS2:
			compareRes = FsmAp::compareTransPartPtr( 0, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::RangeOverlap:
			compareRes = FsmAp::compareTransPartPtr( 
					outPair.s1Tel.trans, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;

		case RangePairIter<TransAp>::BreakS1:
		case RangePairIter<TransAp>::BreakS2:
			break;
		}
	}

	/* Test eof targets. */
	if ( state1->eofTarget == 0 && state2->eofTarget != 0 )
		return -1;
	else if ( state1->eofTarget != 0 && state2->eofTarget == 0 )
		return 1;
	else if ( state1->eofTarget != 0 ) {
		/* Both eof targets are set. */
		compareRes = CmpOrd< MinPartition* >::compare( 
			state1->eofTarget->alg.partition, state2->eofTarget->alg.partition );
		if ( compareRes != 0 )
			return compareRes;
	}

	return 0;
}

/* Compare class for the sort that does the partitioning. */
bool MarkCompare::shouldMark( MarkIndex &markIndex, const StateAp *state1, 
			const StateAp *state2 )
{
	/* Use a pair iterator to get the transition pairs. */
	RangePairIter<TransAp> outPair( ctx, state1->outList.head, state2->outList.head );
	for ( ; !outPair.end(); outPair++ ) {
		switch ( outPair.userState ) {

		case RangePairIter<TransAp>::RangeInS1:
			if ( FsmAp::shouldMarkPtr( markIndex, outPair.s1Tel.trans, 0 ) )
				return true;
			break;

		case RangePairIter<TransAp>::RangeInS2:
			if ( FsmAp::shouldMarkPtr( markIndex, 0, outPair.s2Tel.trans ) )
				return true;
			break;

		case RangePairIter<TransAp>::RangeOverlap:
			if ( FsmAp::shouldMarkPtr( markIndex,
					outPair.s1Tel.trans, outPair.s2Tel.trans ) )
				return true;
			break;

		case RangePairIter<TransAp>::BreakS1:
		case RangePairIter<TransAp>::BreakS2:
			break;
		}
	}

	return false;
}

/*
 * Transition Comparison.
 */

int FsmAp::comparePart( TransAp *trans1, TransAp *trans2 )
{
	/* Use a pair iterator to get the transition pairs. */
	ValPairIter<CondAp> outPair( trans1->condList.head, trans2->condList.head );
	for ( ; !outPair.end(); outPair++ ) {
		switch ( outPair.userState ) {

		case ValPairIter<CondAp>::RangeInS1: {
			int compareRes = FsmAp::compareCondPartPtr( outPair.s1Tel.trans, 0 );
			if ( compareRes != 0 )
				return compareRes;
			break;
		}

		case ValPairIter<CondAp>::RangeInS2: {
			int compareRes = FsmAp::compareCondPartPtr( 0, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;
		}

		case ValPairIter<CondAp>::RangeOverlap: {
			int compareRes = FsmAp::compareCondPartPtr( 
					outPair.s1Tel.trans, outPair.s2Tel.trans );
			if ( compareRes != 0 )
				return compareRes;
			break;
		}

		case ValPairIter<CondAp>::BreakS1:
		case ValPairIter<CondAp>::BreakS2:
			break;
		}
	}

	return 0;
}

/* Compare target partitions. Either pointer may be null. */
int FsmAp::compareTransPartPtr( TransAp *trans1, TransAp *trans2 )
{
	if ( trans1 != 0 ) {
		/* If trans1 is set then so should trans2. The initial partitioning
		 * guarantees this for us. */
		return comparePart( trans1, trans2 );
	}

	return 0;
}

int FsmAp::compareCondPartPtr( CondAp *trans1, CondAp *trans2 )
{
	if ( trans1 != 0 ) {
		/* If trans1 is set then so should trans2. The initial partitioning
		 * guarantees this for us. */
		if ( trans1->toState == 0 && trans2->toState != 0 )
			return -1;
		else if ( trans1->toState != 0 && trans2->toState == 0 )
			return 1;
		else if ( trans1->toState != 0 ) {
			/* Both of targets are set. */
			return CmpOrd< MinPartition* >::compare( 
				trans1->toState->alg.partition, trans2->toState->alg.partition );
		}
	}
	return 0;
}


/* Compares two transition pointers according to priority and functions.
 * Either pointer may be null. Does not consider to state or from state. */
int FsmAp::compareTransDataPtr( TransAp *trans1, TransAp *trans2 )
{
	if ( trans1 == 0 && trans2 != 0 )
		return -1;
	else if ( trans1 != 0 && trans2 == 0 )
		return 1;
	else if ( trans1 != 0 ) {
		/* Both of the transition pointers are set. */
		int compareRes = compareTransData( trans1, trans2 );
		if ( compareRes != 0 )
			return compareRes;
	}
	return 0;
}

/* Compares two transition pointers according to priority and functions.
 * Either pointer may be null. Does not consider to state or from state. */
int FsmAp::compareCondDataPtr( CondAp *trans1, CondAp *trans2 )
{
	if ( trans1 == 0 && trans2 != 0 )
		return -1;
	else if ( trans1 != 0 && trans2 == 0 )
		return 1;
	else if ( trans1 != 0 ) {
		/* Both of the transition pointers are set. */
		int compareRes = compareCondData( trans1, trans2 );
		if ( compareRes != 0 )
			return compareRes;
	}
	return 0;
}

/* Compares two transitions according to target state, priority and functions.
 * Does not consider from state. Either of the pointers may be null. */
int FsmAp::compareFullPtr( TransAp *trans1, TransAp *trans2 )
{
	std::cerr << "FIXME: " << __PRETTY_FUNCTION__ << std::endl;

	if ( (trans1 != 0) ^ (trans2 != 0) ) {
		/* Exactly one of the transitions is set. */
		if ( trans1 != 0 )
			return -1;
		else
			return 1;
	}
	else if ( trans1 != 0 ) {
		/* Both of the transition pointers are set. Test target state,
		 * priority and funcs. */
		if ( trans1->condList.head->toState < trans2->condList.head->toState )
			return -1;
		else if ( trans1->condList.head->toState > trans2->condList.head->toState )
			return 1;
		else if ( trans1->condList.head->toState != 0 ) {
			/* Test transition data. */
			int compareRes = compareTransData( trans1, trans2 );
			if ( compareRes != 0 )
				return compareRes;
		}
	}
	return 0;
}


bool FsmAp::shouldMarkPtr( MarkIndex &markIndex, TransAp *trans1, 
				TransAp *trans2 )
{
	std::cerr << "FIXME: " << __PRETTY_FUNCTION__ << std::endl;

	if ( (trans1 != 0) ^ (trans2 != 0) ) {
		/* Exactly one of the transitions is set. The initial mark round
		 * should rule out this case. */
		assert( false );
	}
	else if ( trans1 != 0 ) {
		/* Both of the transitions are set. If the target pair is marked, then
		 * the pair we are considering gets marked. */
		return markIndex.isPairMarked( trans1->condList.head->toState->alg.stateNum, 
				trans2->condList.head->toState->alg.stateNum );
	}

	/* Neither of the transitiosn are set. */
	return false;
}


