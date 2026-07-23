#include "pch.h"



void helloworld()
{
	zcutPrintf(_T("\nHello world!"));
}


int showhello(struct resbuf *rb)
{
	zds_printf(_T("hello"));
	zds_retvoid();
	return RTNORM;
}

int showhellocmd(struct resbuf *rb)
{
	zds_printf(_T("hello"));
	zds_retvoid();
	return RTNORM;
}
