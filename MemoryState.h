#ifndef MemoryState_H
#define MemoryState_H

#include <QtGui>
#include "Math.h"

class Loader;

class MemoryState {
public:
     MemoryState();
    ~MemoryState();

    bool	openPipe(int argc, char *argv[]);

    enum Visualization {
	LINEAR,
	BLOCK
    };
    void	setVisualization(Visualization vis)
		{ myVisualization = vis; }
    void	fillImage(QImage &image) const;

    void	updateAddress(uint64 addr, int size, char type);

private:
    void	fillLinear(QImage &image) const;
    void	fillRecursiveBlock(QImage &image) const;

    typedef uint32	State;

    static const State	theStale    = ~(State)0;
    static const State	theHalfLife = theStale>>1;
    static const State	theFullLife = theStale-1;

    static const int	theAllBits = 36;
    static const uint64	theAllSize = 1L << theAllBits;
    static const uint64	theAllMask = theAllSize-1;

    static const int	theTopBits = 18;
    static const uint64	theTopSize = 1L << theTopBits;
    static const uint64	theTopMask = theTopSize-1;

    static const int	theBottomBits = theAllBits-theTopBits;
    static const uint64	theBottomSize = 1L << theBottomBits;
    static const uint64	theBottomMask = theBottomSize-1;

    struct StateArray {
	StateArray()
	{
	    memset(myState, 0, theBottomSize*sizeof(State));
	    memset(myType, 0, theBottomSize*sizeof(char));
	}

	State	myState[theBottomSize];
	char	myType[theBottomSize];
    };

    int		topIndex(uint64 addr) const
		{ return (addr >> (32-theTopBits)) & theTopMask; }
    int		bottomIndex(uint64 addr) const
		{ return addr & theBottomMask; }

    State	getEntry(uint64 addr) const
		{
		    StateArray	*row = myTable[topIndex(addr)];
		    return row ? row->myState[bottomIndex(addr)] : 0;
		}
    void	setEntry(uint64 addr, State val, char type)
		{
		    StateArray	*&row = myTable[topIndex(addr)];
		    int		  idx = bottomIndex(addr);
		    if (!row)
			row = new StateArray;
		    row->myState[idx] = val;
		    row->myType[idx] = type;
		}

    uint32	mapColor(State val, char type) const
		{
		    uint32	diff;

		    if (val != theStale)
		    {
			if (myTime >= val)
			    diff = myTime - val + 1;
			else
			    diff = val - myTime + 1;
		    }
		    else
			diff = theHalfLife;

		    diff <<= 8*(sizeof(uint32)-sizeof(State));

		    uint32	bits = __builtin_clz(diff);
		    uint32	clr = bits*8;

		    if (bits > 28)
			clr += ~(diff << (bits-28)) & 7;
		    else
			clr += ~(diff >> (28-bits)) & 7;

		    return type == 'I' ? myILut[clr] :
			(type == 'L' ? myRLut[clr] : myWLut[clr]);
		}

    // A class to iterate over only non-zero state values
    class StateIterator {
    public:
	StateIterator(const MemoryState *state)
	    : myState(state)
	    , myTop(0)
	    , myBottom(0)
	    , myEmptyCount(0)
	{
	}

	void	rewind()
		{
		    myTop = 0;
		    myBottom = 0;
		    skipEmpty();
		}
	bool	atEnd() const
		{
		    return myTop >= theTopSize;
		}
	void	advance()
		{
		    myBottom++;
		    skipEmpty();
		}

	State	state() const	{ return table(myTop)->myState[myBottom]; }
	char	type() const	{ return table(myTop)->myType[myBottom]; }
	uint64	nempty() const	{ return myEmptyCount; }

	void	setState(State val)
		{ myState->myTable[myTop]->myState[myBottom] = val; }

    private:
	const StateArray	*table(int top)	const
				 { return myState->myTable[top]; }

	void	skipEmpty()
		{
		    myEmptyCount = 0;
		    for (; myTop < theTopSize; myTop++)
		    {
			if (table(myTop))
			{
			    for (; myBottom < theBottomSize; myBottom++)
			    {
				if (table(myTop)->myState[myBottom])
				    return;
				myEmptyCount++;
			    }
			}
			else
			    myEmptyCount += theBottomSize;
			myBottom = 0;
		    }
		}

    private:
	const MemoryState	*myState;
	uint64			 myTop;
	uint64			 myBottom;
	uint64			 myEmptyCount;
    };

private:
    // Raw memory state
    StateArray	*myTable[theTopSize];
    State	 myTime;	// Rolling counter
    uint64	 myHRTime;

    // Loader
    Loader	*myLoader;

    // Display LUT
    uint32	 myILut[256];
    uint32	 myRLut[256];
    uint32	 myWLut[256];

    // The number of low-order bits to ignore.  This value determines the
    // resolution and memory use for the profile.
    int		 myIgnoreBits;

    Visualization	myVisualization;
};

#endif
