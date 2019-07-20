/*
	slm1 utility global routines Library
*/


#include <stdlib.h>
#include <flext.h>
#include "math.h"

#include "slm1.h"


/* met à zero un vecteur */

void vzero_f( float *f, int n)
{
	float *p;
	
	p = f;
	while (n--) *p++ = 0.;
}

void vzero( double *f, int n)
{
	double *p;
	
	p = f;
	while (n--) *p++ = 0.;
}

/* copie un vecteur */

void vcopy_f(float *f, float *g, int n)
{
  float *nouveau, *old;
  nouveau = g;
  old = f;
  while (n--) *nouveau++ = *old++;
}

void vcopy(double *f, double *g, int n)
{
  double *nouveau, *old;
  nouveau = g;
  old = f;
  while (n--) *nouveau++ = *old++;
}



/* vérifie si n est une puissance de 2 */

int powerof2p(long n)
{
	long exp = -1,nn = n;
	while (nn)
	{
		nn >>= 1;
		exp++;
	}
	return (n == (1 << exp));
}

/*
#define RAND_MAX 32768.

long state;
short randi()
{
	state = state * 210427233L + 1489352071L;
	return ((state >> 16) & 0x7fff);
}
*/


/******************
 Interpolations
******************/

#if 1 //PD_ARRAY_DOUBLE_PRECISION
t_sample linear_interpol (t_sample *buf, float alpha) /*NEWTON*/
{
	int alpha0 = floor(alpha);
	t_sample beta0 = *(buf + (alpha0 * 2));
	t_sample diff1 = buf[(alpha0 * 2) + 2] - beta0;
	float npoly1 = alpha - alpha0;
//	post("M %f %f %f ",beta0,diff1,npoly1);
	return(beta0 + diff1 * npoly1);
}
t_sample square_interpol (t_sample *buf, float alpha) /*NEWTON - GREGORY*/
{
	int alpha0 = floor(alpha);
	int alpha02 = 2 * alpha0;
	t_sample beta0 = buf[alpha02];
	t_sample beta1 = buf[alpha02 + 2];
	t_sample beta2 = buf[alpha02 + 4];
	t_sample diff1 = beta1 - beta0;
	float npoly1 = alpha - alpha0;
	t_sample diff2i = beta2 - beta1 ;
	t_sample diff2 = diff2i - diff1;
	float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;

	return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}
t_sample cubic_interpol (t_sample *buf, float alpha) /*NEWTON - GREGORY*/
{
	int alpha0 = floor(alpha) - 1;   /* translation */
	int alpha02 = 2 * alpha0;
	t_sample beta0 = buf[alpha02];
	t_sample beta1 = buf[alpha02 + 2];
	t_sample beta2 = buf[alpha02 + 4];
	t_sample diff1 = beta1 - beta0;
	float npoly1 = alpha - alpha0 + 1;
	t_sample diff2i = beta2 - beta1 ;
	t_sample diff2 = diff2i - diff1;
	float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
	t_sample diff3 = buf[alpha02 + 6] - beta2 - diff2i - diff2;
	float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */

	return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}
#else
t_sample linear_interpol (t_sample *buf, float alpha) /*NEWTON*/
{
	int alpha0 = floor(alpha);
	t_sample beta0 = *(buf + alpha0);
	t_sample diff1 = buf[alpha0 + 1] - beta0;
	float npoly1 = alpha - alpha0;
//	post("M %f %f %f ",beta0,diff1,npoly1);
	return(beta0 + diff1 * npoly1);
}
t_sample square_interpol (t_sample *buf, float alpha) /*NEWTON - GREGORY*/
{
	int alpha0 = floor(alpha);
	t_sample beta0 = buf[alpha0];
	t_sample beta1 = buf[alpha0 + 1];
	t_sample beta2 = buf[alpha0 + 2];
	t_sample diff1 = beta1 - beta0;
	float npoly1 = alpha - alpha0;
	t_sample diff2i = beta2 - beta1 ;
	t_sample diff2 = diff2i - diff1;
	float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;

	return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}
t_sample cubic_interpol (t_sample *buf, float alpha) /*NEWTON - GREGORY*/
{
	int alpha0 = floor(alpha) - 1;   /* translation */
	t_sample beta0 = buf[alpha0];
	t_sample beta1 = buf[alpha0 + 1];
	t_sample beta2 = buf[alpha0 + 2];
	t_sample diff1 = beta1 - beta0;
	float npoly1 = alpha - alpha0 + 1;
	t_sample diff2i = beta2 - beta1 ;
	t_sample diff2 = diff2i - diff1;
	float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
	t_sample diff3 = buf[alpha0 + 3] - beta2 - diff2i - diff2;
	float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */

	return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}
#endif

/******************
Interpolations for multi channel buffers
******************/

t_sample linear_interpol_n (t_sample *buf, int alpha0, float npoly1, int nchans) /*NEWTON*/
{
	t_sample beta0 = *(buf + alpha0);
	t_sample diff1 = buf[alpha0 + (1 * nchans)] - beta0;
//	post("S %f %f %f %f ",alpha, beta0,diff1,npoly1);
	return(beta0 + diff1 * npoly1);
}
	

t_sample square_interpol_n (t_sample *buf, int alpha0, float npoly1, int nchans) /*NEWTON - GREGORY*/
{
	t_sample beta0 = buf[alpha0];
	t_sample beta1 = buf[alpha0 + 1 * nchans];
	t_sample beta2 = buf[alpha0 + 2 * nchans];
	t_sample diff1 = beta1 - beta0;
	t_sample diff2i = beta2 - beta1 ;
	t_sample diff2 = diff2i - diff1;
	float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
	
	return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}
t_sample cubic_interpol_n (t_sample *buf,int alpha0, float npoly1, int nchans) /*NEWTON - GREGORY*/
{
	alpha0 = alpha0 - 1 * nchans;   /* translation */
	t_sample beta0 = buf[alpha0];
	t_sample beta1 = buf[alpha0 + 1 * nchans];
	t_sample beta2 = buf[alpha0 + 2 * nchans];
	t_sample diff1 = beta1 - beta0;
	npoly1 = npoly1 ;
	t_sample diff2i = beta2 - beta1 ;
	t_sample diff2 = diff2i - diff1;
	float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
	t_sample diff3 = buf[alpha0 + 3 * nchans] - beta2 - diff2i - diff2;
	float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
	
	return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}


/*********************
	Fenetres
**********************/
void triangular_window(t_sample *wind, int size)
{
	int i,half = size / 2.;
	for(i = 0;i < half ;i++)
		*wind++ = (float)i / (float) half;
	for(i = half;i < size ;i++)
		*wind++ = (float)(- 2. * i / size + 2.);

}

void rectangular_window(t_sample *wind, int size)
{
	int i;
	for(i = 0;i < 50 ;i++)
		*wind++ = (float)i/50.;
	for(i = 50;i < size - 50 ;i++)
		*wind++ = 1.;
	for(i = size - 50;i < size ;i++)
		*wind++ = (float)((float)-i + (float)size) / 50.;
}

void cresc_window(t_sample *wind, int size)
{
	int i;
	float iscale;
	for (i = 0; i < size; i++)
        {iscale=(float)i*50./(float)size - 40.;/* from -40 to +10 dB */
        *wind++ = pow(10.,(iscale/20.))/3.17;}

}
void decresc_window(t_sample *wind, int size)
{
	int i;
	float iscale;
	for (i = 0; i < size; i++)
        {iscale=(float)i*-50./(float)size + 10.;/* from +10 tO -40  DB */
        *wind++ = pow(10.,(iscale/20.))/3.17;}
}
void hamming_window(t_sample *wind, int size)
{
	int i,half = size / 2.;
	float pi =  4 * atan(1.);

		for(i = 0;i < size ;i++)
		*wind++ = (cos(pi + pi * ((float)i / (float)half)) + 1.) / 2.;

}
void hamming32_window(t_sample *wind, int size)
{
	int i,half = size / 2.;
	float pi =  4 * atan(1.);

	for (i = 0; i < size; i++)
        if (i < half / 32)
            *wind++ =(cos(pi + pi * (i / ((float)half / 32.))) + 1.) / 2.;
        else if (i >= size - (half / 32))
            *wind++ =(cos(pi + pi * (((float)i - (float)half) / ((float)half / 32.))) + 1.) / 2.;
        else
	        *wind++ =1.;
}

long  ilog2(long n)
{
	long ret = -1;
	while (n) {
		n >>= 1;
		ret++;
	}
	return (ret);
}



