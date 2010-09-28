#define	FOR	57346
#define	IN	57347
#define	WHILE	57348
#define	IF	57349
#define	NOT	57350
#define	TWIDDLE	57351
#define	BANG	57352
#define	SUBSHELL	57353
#define	SWITCH	57354
#define	FN	57355
#define	WORD	57356
#define	REDIR	57357
#define	DUP	57358
#define	PIPE	57359
#define	FANIN	57360
#define	FANOUT	57361
#define	SUB	57362
#define	SIMPLE	57363
#define	FILTER	57364
#define	ARGLIST	57365
#define	WORDS	57366
#define	BRACE	57367
#define	PAREN	57368
#define	PCMD	57369
#define	PIPEFD	57370
#define	ANDAND	57371
#define	OROR	57372
#define	COUNT	57373

#line	13	"/usr/npe/src/cmd/pushu/syn.y"
#include "rc.h"
#include "fns.h"

#line	16	"/usr/npe/src/cmd/pushu/syn.y"
typedef union {
	struct tree *tree;
} YYSTYPE;
extern	int	yyerrflag;
#ifndef	YYMAXDEPTH
#define	YYMAXDEPTH	150
#endif
YYSTYPE	yylval;
YYSTYPE	yyval;
#define YYEOFCODE 1
#define YYERRCODE 2
short	yyexca[] =
{-1, 0,
	1, 1,
	-2, 18,
-1, 1,
	1, -1,
	-2, 0,
};
#define	YYNPROD	72
#define	YYPRIVATE 57344
#define	YYLAST	352
short	yyact[] =
{
  67,   3,  71,  65,  66,   3,  33,  41,  34,  33,
  40,  34,  70,  61,  62,  63,  64,  42,  94,  31,
  32,  17,  31,  32,  36,  95,  29,  30,  21,  29,
  30,  78,  79,  80,  81,  82,  12,  28,  46,  46,
  46,  90,  37,  41, 104,  93,  86,  59,  46,  99,
  39,  46,  46,  46,  89,  44,  58,  60,  33,  43,
  34,   2,  83,   5,  91,  72,  35,  46,  74,  75,
  76,  46,  88,  96,  37,  38,  20, 105,  33, 101,
  34, 113, 117,  73,  72, 102, 103,  85,  87, 107,
  77,  31,  32,  84,  46,   1,  45,  18,  10,  46,
  46, 106, 111, 110,  90,  13,  68,  46,   0, 112,
  33,   0,  34,   0, 116,   0,  97,  98, 118,  46,
  46,  73,   0,  31,  32,  69,   4,   0,   0,  92,
   4,   0,   0,   0,   0,   0, 109,   0,   0,  46,
   0,  46,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 108,  47,  48,  49,  50,  51,  52,
  53,  54,  55,  56,  25,  57,   0,  47,  48,  49,
  50,  51,  52,  53,  54,  55,  56,  25,  57, 115,
   0,   0,   0,  22,  24,  23,   0,   0,   0,   0,
   0,  27, 114,  26,   0,   0,  22,  24,  23,   0,
   0,   0,   0,   0,  27,   0,  26,  47,  48,  49,
  50,  51,  52,  53,  54,  55,  56,  25,  57,  47,
  48,  49,  50,  51,  52,  53,  54,  55,  56,  25,
  57,   0, 100,   0,   0,   0,  22,  24,  23,   0,
   0,   0,   0,   0,  27,   0,  26,   0,  22,  24,
  23,   0,   0,   0,  17,   0,  27,   0,  26,  47,
  48,  49,  50,  51,  52,  53,  54,  55,  56,  25,
  19,  20,   7,   0,   8,   6,   0,  11,  14,  15,
   9,  16,  25,  19,  20,   0,   0,   0,  22,  24,
  23,   0,   0,   0,   0,   0,  27,   0,  26,   0,
   0,  22,  24,  23,   0,   0,   0,  17,   0,  27,
   0,  26,  47,  48,  49,  50,  51,  52,  53,  54,
  55,  56,  25,  57,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,  22,  24,  23,   0,   0,   0,   0,   0,  27,
   0,  26
};
short	yypact[] =
{
 268,-1000,   1,  -8, 268,  60,   2, -24, -34, 308,
 255, 308, 268, 268, 268, 268,-1000, 268, -30, 215,
-1000,-1000, 308, 308, 308,-1000, -18,-1000,-1000,-1000,
-1000, 268, 268, 268, 268,-1000,-1000,  60, 308,-1000,
-1000, 268, 308,-1000,   9,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000, -18,   9,-1000,
   9,  41,  41,  41,  41, 215, -22, -11, 268,-1000,
 308, 308,   9,-1000,  29,-1000,-1000,-1000, 203,  41,
  41,-1000,  61,-1000, 268, 268,  15,  72, 268, -18,
 308, 308,-1000,   9,-1000,-1000,-1000,   9,-1000,-1000,
-1000, 268,  93,  93,-1000,-1000,-1000,  93,-1000,-1000,
 163,-1000, 150, 268,-1000,-1000,  93, 268,  93
};
short	yypgo[] =
{
   0,  61,  50,  63,   4, 125, 106, 105,  24,  36,
   0,  98,  97,  45,  28,  96,   3,  95,  93,  87,
  82,  81,  72,  54
};
short	yyr1[] =
{
   0,  17,  17,   1,   1,   4,   4,   5,   5,   6,
   6,   3,   2,   7,   8,   8,   9,   9,  10,  10,
  18,  10,  19,  10,  20,  10,  21,  10,  22,  10,
  23,  10,  10,  10,  10,  10,  10,  10,  10,  10,
  10,  10,  10,  10,  11,  11,  11,  12,  12,  13,
  13,  13,  14,  14,  14,  14,  14,  14,  14,  14,
  15,  15,  15,  15,  15,  15,  15,  15,  15,  15,
  16,  16
};
short	yyr2[] =
{
   0,   0,   2,   1,   2,   1,   2,   2,   2,   1,
   2,   3,   3,   3,   0,   2,   2,   1,   0,   2,
   0,   4,   0,   4,   0,   8,   0,   6,   0,   4,
   0,   4,   1,   3,   3,   3,   3,   5,   2,   2,
   2,   2,   3,   2,   1,   2,   2,   1,   3,   1,
   1,   3,   2,   5,   2,   2,   1,   2,   3,   2,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   0,   2
};
short	yychk[] =
{
-1000, -17,  -1, -10,  -5,  -3,   7,   4,   6,  12,
 -11,   9,  -9,  -7,  10,  11,  13,  39, -12,  15,
  16, -14,  33,  35,  34,  14,  43,  41,  36,  37,
  38,  30,  31,  17,  19,  -1,  -8,  -9,  15,  -2,
   8,  41,  41,  -2, -13, -15, -14,   4,   5,   6,
   7,   8,   9,  10,  11,  12,  13,  15, -13,  -9,
 -13, -10, -10, -10, -10, -16,  -4, -10,  -6,  -5,
  42,  32, -13,  -3, -13, -13, -13,  -3, -16, -10,
 -10, -10, -10,  -8, -18, -19,  -4, -13, -22, -23,
  32, -16,  -3, -13,  40,  36,  -4, -13, -13,  20,
  29,  18, -10, -10,  29,   5,  29, -10,  -3, -13,
 -16, -10, -16, -21,  29,  29, -10, -20, -10
};
short	yydef[] =
{
  -2,  -2,   0,   3,  18,  14,   0,   0,   0,   0,
  32,   0,  18,  18,  18,  18,  70,  18,  44,   0,
  17,  47,   0,   0,   0,  56,   0,  70,   2,   7,
   8,  18,  18,  18,  18,   4,  19,  14,   0,  20,
  22,  18,   0,  28,  30,  49,  50,  60,  61,  62,
  63,  64,  65,  66,  67,  68,  69,   0,  45,  46,
  70,  38,  39,  40,  41,  43,   0,   5,  18,   9,
   0,   0,  16,  59,  52,  54,  55,  57,   0,  34,
  35,  36,   0,  15,  18,  18,   0,   0,  18,   0,
   0,  33,  42,  71,  11,  10,   6,  13,  48,  70,
  58,  18,  21,  23,  12,  70,  26,  29,  31,  51,
   0,  37,   0,  18,  53,  24,  27,  18,  25
};
short	yytok1[] =
{
   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  36,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,  35,   0,  33,   0,  38,   0,
  41,  29,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,  37,
   0,  42,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,  32,   0,  43,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,  39,   0,  40
};
short	yytok2[] =
{
   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,
  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,
  22,  23,  24,  25,  26,  27,  28,  30,  31,  34
};
long	yytok3[] =
{
   0
};
#define YYFLAG 		-1000
#define	yyclearin	yychar = -1
#define	yyerrok		yyerrflag = 0

#ifdef	yydebug
#include	"y.debug"
#else
#define	yydebug		0
char*	yytoknames[1];		/* for debugging */
char*	yystates[1];		/* for debugging */
#endif

/*	parser for yacc output	*/

int	yynerrs = 0;		/* number of errors */
int	yyerrflag = 0;		/* error recovery flag */

extern	int	fprint(int, char*, ...);
extern	int	sprint(char*, char*, ...);

char*
yytokname(int yyc)
{
	static char x[16];

	if(yyc > 0 && yyc <= sizeof(yytoknames)/sizeof(yytoknames[0]))
	if(yytoknames[yyc-1])
		return yytoknames[yyc-1];
	sprint(x, "<%d>", yyc);
	return x;
}

char*
yystatname(int yys)
{
	static char x[16];

	if(yys >= 0 && yys < sizeof(yystates)/sizeof(yystates[0]))
	if(yystates[yys])
		return yystates[yys];
	sprint(x, "<%d>\n", yys);
	return x;
}

long
yylex1(void)
{
	long yychar;
	long *t3p;
	int c;

	yychar = yylex();
	if(yychar <= 0) {
		c = yytok1[0];
		goto out;
	}
	if(yychar < sizeof(yytok1)/sizeof(yytok1[0])) {
		c = yytok1[yychar];
		goto out;
	}
	if(yychar >= YYPRIVATE)
		if(yychar < YYPRIVATE+sizeof(yytok2)/sizeof(yytok2[0])) {
			c = yytok2[yychar-YYPRIVATE];
			goto out;
		}
	for(t3p=yytok3;; t3p+=2) {
		c = t3p[0];
		if(c == yychar) {
			c = t3p[1];
			goto out;
		}
		if(c == 0)
			break;
	}
	c = 0;

out:
	if(c == 0)
		c = yytok2[1];	/* unknown char */
	if(yydebug >= 3)
		fprint(2, "lex %.4lux %s\n", yychar, yytokname(c));
	return c;
}

int
yyparse(void)
{
	struct
	{
		YYSTYPE	yyv;
		int	yys;
	} yys[YYMAXDEPTH], *yyp, *yypt;
	short *yyxi;
	int yyj, yym, yystate, yyn, yyg;
	long yychar;
	YYSTYPE save1, save2;
	int save3, save4;

	save1 = yylval;
	save2 = yyval;
	save3 = yynerrs;
	save4 = yyerrflag;

	yystate = 0;
	yychar = -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyp = &yys[-1];
	goto yystack;

ret0:
	yyn = 0;
	goto ret;

ret1:
	yyn = 1;
	goto ret;

ret:
	yylval = save1;
	yyval = save2;
	yynerrs = save3;
	yyerrflag = save4;
	return yyn;

yystack:
	/* put a state and value onto the stack */
	if(yydebug >= 4)
		fprint(2, "char %s in %s", yytokname(yychar), yystatname(yystate));

	yyp++;
	if(yyp >= &yys[YYMAXDEPTH]) {
		yyerror("yacc stack overflow");
		goto ret1;
	}
	yyp->yys = yystate;
	yyp->yyv = yyval;

yynewstate:
	yyn = yypact[yystate];
	if(yyn <= YYFLAG)
		goto yydefault; /* simple state */
	if(yychar < 0)
		yychar = yylex1();
	yyn += yychar;
	if(yyn < 0 || yyn >= YYLAST)
		goto yydefault;
	yyn = yyact[yyn];
	if(yychk[yyn] == yychar) { /* valid shift */
		yychar = -1;
		yyval = yylval;
		yystate = yyn;
		if(yyerrflag > 0)
			yyerrflag--;
		goto yystack;
	}

yydefault:
	/* default state action */
	yyn = yydef[yystate];
	if(yyn == -2) {
		if(yychar < 0)
			yychar = yylex1();

		/* look through exception table */
		for(yyxi=yyexca;; yyxi+=2)
			if(yyxi[0] == -1 && yyxi[1] == yystate)
				break;
		for(yyxi += 2;; yyxi += 2) {
			yyn = yyxi[0];
			if(yyn < 0 || yyn == yychar)
				break;
		}
		yyn = yyxi[1];
		if(yyn < 0)
			goto ret0;
	}
	if(yyn == 0) {
		/* error ... attempt to resume parsing */
		switch(yyerrflag) {
		case 0:   /* brand new error */
			yyerror("syntax error");
			yynerrs++;
			if(yydebug >= 1) {
				fprint(2, "%s", yystatname(yystate));
				fprint(2, "saw %s\n", yytokname(yychar));
			}

		case 1:
		case 2: /* incompletely recovered error ... try again */
			yyerrflag = 3;

			/* find a state where "error" is a legal shift action */
			while(yyp >= yys) {
				yyn = yypact[yyp->yys] + YYERRCODE;
				if(yyn >= 0 && yyn < YYLAST) {
					yystate = yyact[yyn];  /* simulate a shift of "error" */
					if(yychk[yystate] == YYERRCODE)
						goto yystack;
				}

				/* the current yyp has no shift onn "error", pop stack */
				if(yydebug >= 2)
					fprint(2, "error recovery pops state %d, uncovers %d\n",
						yyp->yys, (yyp-1)->yys );
				yyp--;
			}
			/* there is no state on the stack with an error shift ... abort */
			goto ret1;

		case 3:  /* no shift yet; clobber input char */
			if(yydebug >= 2)
				fprint(2, "error recovery discards %s\n", yytokname(yychar));
			if(yychar == YYEOFCODE)
				goto ret1;
			yychar = -1;
			goto yynewstate;   /* try again in the same state */
		}
	}

	/* reduction by production yyn */
	if(yydebug >= 2)
		fprint(2, "reduce %d in:\n\t%s", yyn, yystatname(yystate));

	yypt = yyp;
	yyp -= yyr2[yyn];
	yyval = (yyp+1)->yyv;
	yym = yyn;

	/* consult goto table to find next state */
	yyn = yyr1[yyn];
	yyg = yypgo[yyn];
	yyj = yyg + yyp->yys + 1;

	if(yyj >= YYLAST || yychk[yystate=yyact[yyj]] != -yyn)
		yystate = yyact[yyg];
	switch(yym) {
		
case 1:
#line	24	"/usr/npe/src/cmd/pushu/syn.y"
{ return 1;} break;
case 2:
#line	25	"/usr/npe/src/cmd/pushu/syn.y"
{return !compile(yypt[-1].yyv.tree);} break;
case 4:
#line	27	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(';', yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 6:
#line	29	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(';', yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 8:
#line	31	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1('&', yypt[-1].yyv.tree);} break;
case 11:
#line	34	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1(BRACE, yypt[-1].yyv.tree);} break;
case 12:
#line	35	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1(PCMD, yypt[-1].yyv.tree);} break;
case 13:
#line	36	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2('=', yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 14:
#line	37	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=0;} break;
case 15:
#line	38	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung2(yypt[-1].yyv.tree, yypt[-1].yyv.tree->child[0], yypt[-0].yyv.tree);} break;
case 16:
#line	39	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung1(yypt[-1].yyv.tree, yypt[-1].yyv.tree->rtype==HERE?heredoc(yypt[-0].yyv.tree):yypt[-0].yyv.tree);} break;
case 18:
#line	41	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=0;} break;
case 19:
#line	42	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=epimung(yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 20:
#line	43	"/usr/npe/src/cmd/pushu/syn.y"
{skipnl();} break;
case 21:
#line	44	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung2(yypt[-3].yyv.tree, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 22:
#line	45	"/usr/npe/src/cmd/pushu/syn.y"
{skipnl();} break;
case 23:
#line	45	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung1(yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 24:
#line	46	"/usr/npe/src/cmd/pushu/syn.y"
{skipnl();} break;
case 25:
#line	55	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung3(yypt[-7].yyv.tree, yypt[-5].yyv.tree, yypt[-3].yyv.tree ? yypt[-3].yyv.tree : tree1(PAREN, yypt[-3].yyv.tree), yypt[-0].yyv.tree);} break;
case 26:
#line	56	"/usr/npe/src/cmd/pushu/syn.y"
{skipnl();} break;
case 27:
#line	57	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung3(yypt[-5].yyv.tree, yypt[-3].yyv.tree, (struct tree *)0, yypt[-0].yyv.tree);} break;
case 28:
#line	58	"/usr/npe/src/cmd/pushu/syn.y"
{skipnl();} break;
case 29:
#line	59	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung2(yypt[-3].yyv.tree, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 30:
#line	60	"/usr/npe/src/cmd/pushu/syn.y"
{skipnl();} break;
case 31:
#line	61	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(SWITCH, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 32:
#line	62	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=simplemung(yypt[-0].yyv.tree);} break;
case 33:
#line	63	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung2(yypt[-2].yyv.tree, yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 34:
#line	64	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(ANDAND, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 35:
#line	65	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(OROR, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 36:
#line	66	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung2(yypt[-1].yyv.tree, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 37:
#line	67	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung3mp(yypt[-3].yyv.tree, yypt[-4].yyv.tree, yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 38:
#line	68	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung2(yypt[-1].yyv.tree, yypt[-1].yyv.tree->child[0], yypt[-0].yyv.tree);} break;
case 39:
#line	69	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung3(yypt[-1].yyv.tree, yypt[-1].yyv.tree->child[0], yypt[-1].yyv.tree->child[1], yypt[-0].yyv.tree);} break;
case 40:
#line	70	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung1(yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 41:
#line	71	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung1(yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 42:
#line	72	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(FN, yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 43:
#line	73	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1(FN, yypt[-0].yyv.tree);} break;
case 45:
#line	75	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(ARGLIST, yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 46:
#line	76	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(ARGLIST, yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
case 48:
#line	78	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2('^', yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 49:
#line	79	"/usr/npe/src/cmd/pushu/syn.y"
{lastword=1; yypt[-0].yyv.tree->type=WORD;} break;
case 51:
#line	81	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2('^', yypt[-2].yyv.tree, yypt[-0].yyv.tree);} break;
case 52:
#line	82	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1('$', yypt[-0].yyv.tree);} break;
case 53:
#line	83	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(SUB, yypt[-3].yyv.tree, yypt[-1].yyv.tree);} break;
case 54:
#line	84	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1('"', yypt[-0].yyv.tree);} break;
case 55:
#line	85	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1(COUNT, yypt[-0].yyv.tree);} break;
case 57:
#line	87	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1('`', yypt[-0].yyv.tree);} break;
case 58:
#line	88	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree1(PAREN, yypt[-1].yyv.tree);} break;
case 59:
#line	89	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=mung1(yypt[-1].yyv.tree, yypt[-0].yyv.tree); yyval.tree->type=PIPEFD;} break;
case 70:
#line	91	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=(struct tree*)0;} break;
case 71:
#line	92	"/usr/npe/src/cmd/pushu/syn.y"
{yyval.tree=tree2(WORDS, yypt[-1].yyv.tree, yypt[-0].yyv.tree);} break;
	}
	goto yystack;  /* stack new state and value */
}
