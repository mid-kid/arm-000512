/* Resources for byacc. */

#include "SysTypes.r"

/* Version resources. */

resource 'vers' (1)  {
	0,
	0,
	0,
	0,
	verUs,
	VERSION_STRING,
	VERSION_STRING  " (C) University of California"
};

resource 'vers' (2, purgeable)  {
	0,
	0,
	0,
	0,
	verUs,
	VERSION_STRING,
	"byacc " VERSION_STRING " for MPW"
};

#ifdef WANT_CFRG

#include "CodeFragmentTypes.r"

resource 'cfrg' (0) {
	{
		kPowerPC,
		kFullLib,
		kNoVersionNum,kNoVersionNum,
		0,0,
		kIsApp,kOnDiskFlat,kZeroOffset,kWholeFork,
		PROG_NAME
	}
};

#endif /* WANT_CFRG */
