/*USEFUL MACROS*/
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

#define interpol(x1, x2, i) ((x1) * (1.0 - i) + (x2) * i)
#define cent_to_linear(x) (exp(0.00057762265047 * (x)))
#define midi_to_freq(x) (440.0 * exp(0.057762265047 * ((x) - 69.0)))
#define dB_to_linear(x) (exp(0.11512925465 * (x)))

/**************************************************/
float slm1_get_value(struct atom *a);
void vzero(double *f,int n);
void vcopy(double *f, double *g, int n);
void vzero_f(float *f,int n);
void vcopy_f(float *f, float *g, int n);
int powerof2p(long n);
/* double random(double min, double max);
 */

#define PD_ARRAY_DOUBLE_PRECISION 1

t_sample linear_interpol (t_sample *buf, float alpha);
t_sample square_interpol (t_sample *buf, float alpha);
t_sample cubic_interpol (t_sample *buf, float alpha);
t_sample linear_interpol_n (t_sample *buf, int alpha0, float npoly1, int nchans);
t_sample square_interpol_n (t_sample *buf, int alpha0, float npoly1, int nchans);
t_sample cubic_interpol_n (t_sample *buf,int alpha0, float npoly1, int nchans);
void triangular_window (t_sample *wind, int size);
void rectangular_window (t_sample *wind, int size);
void cresc_window (t_sample *wind, int size);
void decresc_window (t_sample *wind, int size);
void hamming_window (t_sample *wind, int size);
void hamming32_window (t_sample *wind, int size);
long ilog2(long n);


