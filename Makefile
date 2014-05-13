# $Id$

PROG=	riffdate
NOMAN=

WARNS?=	3

TESTFILES= \
	DP2M2622.AVI \
	DSCN1060.AVI \
	SDIM8427.AVI

regress:	${PROG}
.for T in ${TESTFILES}
	${.OBJDIR}/${PROG} ${.CURDIR}/testfiles/${T} > ${T}.out
	diff -u ${.CURDIR}/expected/${T} ${T}.out
.endfor

.for T in ${TESTFILES}
CLEANFILES+= ${T}.out
.endfor

.include <bsd.prog.mk>
