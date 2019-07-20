/*
 flext version of the samplor object
 Copyright (c) 2007 Serge Lemouton (lemouton@ircam.fr)
 */

#define FLEXT_ATTRIBUTES 1
#include <flext.h>
// check for appropriate flext version
#if !defined(FLEXT_VERSION) || (FLEXT_VERSION < 401)
#error You need at least flext version 0.4.1
#endif

#include <stdio.h>
#include <math.h>
#include <string>
#include <map>
#include <tr1/unordered_map>
#include "slm1.h"
#include "samplor2.h"
#define VERSION "samplor2 v2.43 - charlie special"
#define MULTIPAN 1
#define DELAY_ACTIVE 1
#define WINAR 1

 std::tr1::unordered_map <std::string , std::pair<long,long> > loopMap ; //because no such thing as loop support in puredata "buffers"

class samplor2:
// A flext dsp external ("tilde object") inherits from the class flext_dsp 
public flext_dsp
	{
		// obligatory flext header (class name,base class name) 
		FLEXT_HEADER_S(samplor2,flext_dsp,setup)
		
	public:
		// constructor 
		samplor2(int numoutputs,int maxvoices);
 		
	protected:
		
		virtual bool CbDsp();
		virtual void CbSignal();

	private:	
		
		t_samplor_inputs inputs;
		t_samplor_params params;			/* samplor parameters */
		t_sample windows[NUM_WINS][WIND_SIZE];/* grain envelope */
		t_samplor_list list;				/* samplor list */
		t_list waiting_notes;				/* notes en attente */    
 		long interpol;						/* special mode */
		long voice_stealing;				/* special mode */
		long loop_xfade;					/* special mode */
		long debug;
		long active_voices;  /* nombre de voix actives */
		long polyphony;  /* nombre de voix actives */
		long num_outputs;			/* nombre de sorties (1,2 ou 3) */
		long stereo_mode;
		t_sample *vectors[SAMPLOR_MAX_OUTCOUNT+1]; 
		long time;
		long count;
		
	//	DEFSIGFUN(samplor_perform0);
		
		DEFSIGCALL(samplordspfun);
		
		
		void samplor_init()
		{ 	
			inputs.offset = 0;
			inputs.dur = 0;
			inputs.attack = 2;
			inputs.decay = 2;
			inputs.sustain = 100;
			inputs.release = 2;
			inputs.transp = 1.;
			inputs.amp = 1.;
			inputs.pan = 0.5;
			inputs.rev = 1.;
			inputs.env = 1;		//window type
			inputs.buf = 0;
			params.sr = DEFAULT_SRATE;
			params.sp = 1 / params.sr;
			debug = 0;
			interpol = 1;		/* default : linear interpolation */
			voice_stealing = 0;	/* default : no voice stealing */
			loop_xfade = 0;	/* default : no voice stealing */
			samplor_windows();
			list_init(&this->waiting_notes);
		}
		
		/* 
		 * make windows
		 */
		void samplor_windows()
		{		
			int size = WIND_SIZE;
			
			triangular_window(windows[1],size);
			rectangular_window(windows[2],size);
			cresc_window(windows[3],size);
			decresc_window(windows[4],size);
			hamming_window(windows[5],size);
			hamming32_window(windows[6],size);
		}
	
		/*
		 * samplor_run_one runs one voice and returns 0 if the voice can be freed and 1
		 * if it is still needed, TOUT EST LA !
		 */
		
		int samplor_run_one(t_samplor_entry *x, t_sample **out, long n, t_sample *windows, long num_outputs,long interpol,long loop_xfade)
		{
			buffer *b = x->buf;
			t_sample *out1 = out[0];
			t_sample sample;
			float *tab,w_f_index,f; 
			long index, index2, w_index, frames, nc;
			t_sample *out2 = out[1];
			t_sample *out3 = out[2];
			t_sample *window = windows + 512 * x->win;
			float xfade_amp;
			long in_xfadeflag = 0;
			int samplecount = 0;
            int m;
			
			if (!b)
				goto zero;
			if (!b->Valid())
				goto zero;
			//tab = b->b_samples;
			tab = (float*) b->Data();
			frames = b->Frames();
			nc = b->Channels(); 
			
#if DELAY_ACTIVE
			if (x->start > 0) 
			{
				int m =  min(n, x->start);
				n -= m;
				out += m;
				x->start -= m;
			}
#endif
			
			if (x->count > 0) 
			{
                if(x->loop_flag == LOOP)
                    m=n;
                else m = min(n, x->count);
				
				x->count -= m;
                
				while (m--) 
				{
					/*BOUCLE*/
					if((x->loop_flag == LOOP)|| (x->loop_flag == FINISHING_LOOP))		
                    {
						if(x->fposition > x->loop_end)  /* on boucle */
						{
							// x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
							// x->loop_end = x->loop_end_d;
							// x->loop_dur = x->loop_dur_d;
							x->fposition -= x->loop_dur;
                            if(x->loop_flag == FINISHING_LOOP)
                                x->loop_flag = 0;
                            
                            in_xfadeflag=0;
                            x->count += 1 + x->loop_dur / x->increment; // I don't really know why the 1 ????
                            }
						else if(loop_xfade && (x->fposition > (x->loop_end - loop_xfade)))
						{
							in_xfadeflag=1;
							xfade_amp = ((x->fposition - x->loop_end) / loop_xfade) + 1.;
						}
						else
							in_xfadeflag=0;
					}
					else if((x->loop_flag == ALLER_RETOUR_LOOP) || (x->loop_flag == IN_ALLER_RETOUR_LOOP))
					{
						if(x->fposition >= x->loop_end)
						{
							x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
							x->loop_dur = x->loop_end - x->loop_beg;
							x->count += x->loop_dur / x->increment;
							x->increment = - x->increment; //l'increment devient négatif
							x->loop_flag = IN_ALLER_RETOUR_LOOP;
						}
						else if((x->fposition <= x->loop_beg) && (x->loop_flag == IN_ALLER_RETOUR_LOOP))
						{
							/* eventually, modify loop points */
							x->loop_end = x->loop_end_d;
							x->loop_dur = x->loop_end - x->loop_beg;
							
							x->increment = - x->increment;//l'increment redevient positif
							x->count += x->loop_dur / x->increment;
						}
					}
                    
                    f = x->fposition;
					index = (long)f;
                    index2 = (long)x->fposition2;
                    
                    x->fposition += x->increment ;
                    x->fposition2 += x->increment;

					/*ECHANTILLON*/
					if(!interpol)
					{
						if (index < 0)
							index = 0;
						else if (index >= frames)
							index = frames - 1;
						if (nc > 1)
							index = index * nc ;
						#if PD_ARRAY_DOUBLE_PRECISION
							sample = tab[index * 2];
						#else
							sample = tab[index];
						#endif
						
						if(in_xfadeflag)  
						{/* do the loop cross fade !*/
							sample *= 1. - xfade_amp;
						#if PD_ARRAY_DOUBLE_PRECISION
							sample +=  xfade_amp * tab[index - x->loop_dur];						
						#else
							sample +=  xfade_amp * tab[index - x->loop_dur];
						#endif
						}
					}
					else
					{
						if (f < 0)
							f = 0;
						else if (f >= frames)
							f = frames - 1;
						if (nc > 1)
							f *=  nc ;
						if(!in_xfadeflag)
						{
							if(interpol == 1)
								sample = linear_interpol (tab,f);
							else if(interpol == 2)
								sample = square_interpol (tab,f);
							else
								sample = cubic_interpol (tab,f);
						}
						else
						/*LOOP CROSSFADE : */
						{
							if(interpol == 1)
							{
								sample = (1. - xfade_amp) * linear_interpol (tab,f);
								sample +=  xfade_amp * linear_interpol (tab,f- x->loop_dur);
							}
							else if(interpol == 2)
							{
								sample = (1. - xfade_amp) * square_interpol (tab,f);
								sample +=  xfade_amp * square_interpol (tab,f- x->loop_dur);
							}
							else 
							{  /* optimisation possible (un seul appel a cubic_interpol()) */
								sample = (1. - xfade_amp) * cubic_interpol (tab,f);
								sample +=  xfade_amp * cubic_interpol (tab,f- x->loop_dur);
							}
						}
					}
					/* AMPLITUDE ENVELOPE :*/
					if(x->win)
					{
						w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
						sample *= x->amplitude * *(window + w_index);
#if WINAR
						if (index < x->attack)
						{
							w_f_index = (float)(index - x->begin) * x->attack_ratio;
						}
						else if (index > x->release)
						{
							w_f_index = (float)( x->end - index) * x->release_ratio;
							//	w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
						}
						else w_f_index = 1.;
						sample *= w_f_index;
#endif				
						
					}
					else
					{
						/* attack-release stuff */
						if (index < x->attack)
						{
							w_f_index = (float)(index - x->begin) * x->attack_ratio;
						}
						else if ((index > x->release) && (x->loop_flag != LOOP)&& (!x->fade_out_time))
                        {
                            w_f_index = (float)( x->end - index) * x->release_ratio;
                        }
						else if (index < x->decay)
						{
							w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
						}
						else w_f_index = x->sustain;
						
						sample *= x->amplitude * w_f_index;
					}
					if(x->fade_out_time) //for clean voice_stealing
					{	
						if(!x->fade_out_end)
							x->fade_out_end = (index + x->fade_out_time);
						sample *= MAX(0.,(float)(x->fade_out_end - index) / (float) x->fade_out_time);
					}
					/*PAN & AUX :*/
					if(num_outputs > 3)
					{
#ifdef MULTIPAN
						out[x->chan2][samplecount] += sample * x->pan;
						out[x->chan][samplecount++] += sample * (1. - x->pan);
#else
						out[x->chan][samplecount++] += sample;
#endif
					}
					else if (num_outputs > 1)
					{
						*out2++ += sample * x->pan;
						if (num_outputs > 2)
							*out3++ += sample * x->rev;
						*out1++ += (1. - x->pan) * sample;
						// *out1++ += x->amplitude * w_f_index;
					}
					else 
						*out1++ += sample;
				}
				return 1;
			}
			else
				zero:
				return 0;
		}
		
		/*
		 * samplor_run_one runs one voice and returns 0 if the voice can be freed and 1
		 * if it is still needed, TOUT EST LA !
		 */
		
		int samplor_run_one_stereo(t_samplor_entry *x, t_sample **out, long n, t_sample *windows, long num_outputs,long interpol,long loop_xfade)
		{
			buffer *b = x->buf;
			t_sample *out1 = out[0];
			t_sample sampleL,sampleR;
			float *tab,w_f_index;
			double f,f2; 
			long index, index2, w_index, frames, nc;
			t_sample *out2 = out[1];
			t_sample *window = windows + 512 * x->win;
			float xfade_amp;
			long in_xfadeflag = 0;
			int samplecount = 0;

			
			if (!b)
				goto zero;
			if (!b->Valid())
				goto zero;
//tab = b->Samples();
			tab = (float *)b->Data();
			frames = b->Frames();
			nc = b->Channels(); 
			
#if DELAY_ACTIVE
			if (x->start > 0) 
			{
				int m =  min(n, x->start);
				n -= m;
				out += m;
				x->start -= m;
			}
#endif
			
			if (x->count > 0) 
				//		if ((x->count > 0) || (x->loop_flag == LOOP))
			{
				int m = min(n, x->count);
				
				x->count -= m;
				while (m--) 
				{
					f = x->fposition;
					index = (long)f;
					x->fposition += x->increment ;
					/*BOUCLE*/
					if(x->loop_flag == LOOP)
					{
						if(x->fposition > x->loop_end)
						{
							x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
							x->loop_end = x->loop_end_d;
							x->loop_dur = x->loop_dur_d;
							x->fposition -= x->loop_dur;
							x->count += x->loop_dur / x->increment;
						}
						else if(loop_xfade && (x->fposition > (x->loop_end - loop_xfade)))
						{
							in_xfadeflag=1;
							xfade_amp = ((x->fposition - x->loop_end) / loop_xfade) + 1.;
						}
						else
							in_xfadeflag=0;
					}
					else if((x->loop_flag == ALLER_RETOUR_LOOP) || (x->loop_flag == IN_ALLER_RETOUR_LOOP))
					{
						if(x->fposition >= x->loop_end)
						{
							x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
							x->loop_dur = x->loop_end - x->loop_beg;
							x->count += x->loop_dur / x->increment;
							x->increment = - x->increment; //l'increment devient négatif
							x->loop_flag = IN_ALLER_RETOUR_LOOP;
						}
						else if((x->fposition <= x->loop_beg) && (x->loop_flag == IN_ALLER_RETOUR_LOOP))
						{
							/* eventually, modify loop points */
							x->loop_end = x->loop_end_d;
							x->loop_dur = x->loop_end - x->loop_beg;
							
							x->increment = - x->increment;//l'increment redevient positif
							x->count += x->loop_dur / x->increment;
						}
					}
					/*ECHANTILLON*/
					if (index < 0)
						index = 0;
					else if (index > frames)
						index = frames - 1;
					index2 = index * nc ;
					if(!interpol)
					{
						sampleR = tab[index2];
						sampleL = tab[index2 + 1];
						if(in_xfadeflag)  
						{/* do the loop cross fade !*/
							sampleL *= 1. - xfade_amp;
							sampleR *= 1. - xfade_amp;
							
							sampleL +=  xfade_amp * tab[index2 - x->loop_dur];
							sampleR +=  xfade_amp * tab[index2 + 1 - x->loop_dur];
						}
					}
					else
					{
						if (f < 0)
							f = 0;
						else if (f > frames)
							f = frames - 3;
						f2 = f - floor(f);
						f = (double) index2;
						f += f2;
						if(!in_xfadeflag)
						{
							if(interpol == 1)
							{
								// sampleR = linear_interpol_n (tab,f,2);
								// sampleL = linear_interpol_n (tab,f + 1.,2);
								sampleR = linear_interpol_n (tab,index2,f2,2);
								sampleL = linear_interpol_n (tab,index2 + 1,f2,2);
							}
							else if(interpol == 2)
							{
								sampleR = square_interpol_n (tab,index2,f2,2);
								sampleL = square_interpol_n (tab,index2 + 1,f2,2);
							}
							else if(interpol == 3)
							{
								sampleR = cubic_interpol_n (tab,index2,f2,2);
								sampleL = cubic_interpol_n (tab,index2 + 1,f2,2);
							}
						}
						else
						/*LOOP CROSSFADE : */
						{
							if(interpol == 1)
							{
								sampleR = (1. - xfade_amp) * linear_interpol_n (tab,index2,f2,2);
								sampleR +=  xfade_amp * linear_interpol_n (tab,index2 - x->loop_dur,f2,2);
								sampleL = (1. - xfade_amp) * linear_interpol_n (tab,index2 + 1,f2,2);
								sampleL +=  xfade_amp * linear_interpol_n (tab,index2 + 1 - x->loop_dur, f2,2);
								
							}
							else if(interpol == 2)
							{
								sampleR = (1. - xfade_amp) * square_interpol_n (tab,index2,f2,2);
								sampleR +=  xfade_amp * square_interpol_n (tab,index2 - x->loop_dur,f2,2);
								sampleL = (1. - xfade_amp) * square_interpol_n (tab,index2 + 1,f2,2);
								sampleL +=  xfade_amp * square_interpol_n (tab,index2 + 1 - x->loop_dur,f2,2);
								
							}
							else if(interpol == 3)
							{  /* optimisation possible (un seul appel a cubic_interpol()) */
								sampleR = (1. - xfade_amp) * cubic_interpol_n (tab,index2,f2,2);
								sampleR +=  xfade_amp * cubic_interpol_n (tab,index2 - x->loop_dur,f2,2);
								sampleL = (1. - xfade_amp) * cubic_interpol_n (tab,index2 + 1,f2,2);
								sampleL +=  xfade_amp * cubic_interpol_n (tab,index2 + 1 - x->loop_dur,f2,2);
								
							}
						}
					}
					
					/* AMPLITUDE ENVELOPE :*/
					if(x->win)
					{
						w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
						sampleL *= x->amplitude * *(window + w_index);
						sampleR *= x->amplitude * *(window + w_index);
#if WINAR
						if (index < x->attack)
						{
							w_f_index = (float)(index - x->begin) * x->attack_ratio;
						}
						else if (index > x->release)
						{
							w_f_index = (float)( x->end - index) * x->release_ratio;
							//	w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
						}
						else w_f_index = 1.;
						sampleL *= w_f_index;
						sampleR *= w_f_index;
						
#endif				
						
					}
					else
					{
						/* attack-release stuff */
						if (index < x->attack)
						{
							w_f_index = (float)(index - x->begin) * x->attack_ratio;
						}
						else if (index > x->release)
						{
							w_f_index = (float)( x->end - index) * x->release_ratio;
							//	w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
						}
						else if (index < x->decay)
						{
							w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
						}
						else w_f_index = x->sustain;
						
						sampleL *= x->amplitude * w_f_index;
						sampleR *= x->amplitude * w_f_index;
						
					}
					if(x->fade_out_time) //for clean voice_stealing
					{	
						if(!x->fade_out_end)
							x->fade_out_end = (index + x->fade_out_time);
						sampleL *= (float)(x->fade_out_end - index) / (float) x->fade_out_time;
						sampleR *= (float)(x->fade_out_end - index) / (float) x->fade_out_time;
						
					}
					
					
					
					
					if(num_outputs > 3)
					{
#ifdef MULTIPAN
						out[x->chan2][samplecount] += sampleL * x->pan;
						out[x->chan][samplecount++] += sampleR * (1. - x->pan);
#else
						out[x->chan][samplecount++] += sampleL;
#endif
					}
					else
					{
						
					*out2++ += sampleL * x->pan;
					*out1++ += (1. - x->pan) * sampleR;
					}
				}
				return 1;
			}
			else
				zero:
				return 0;
		}
		
		int samplor_run_one_lite(t_samplor_entry *x, t_sample **out, long n,long interpol)
		{
			buffer *b = x->buf;
			t_sample *out1 = out[0];
			t_sample sample;
			//float *tab,w_f_index; 
			t_sample *tab,w_f_index;
			float f;
			long index, frames, nc;
			
			if (!b)
				goto zero;
			if (!b->Valid())
				goto zero;
			// tab = b->b_samples;
			tab=(float *)b->Data();
			frames = b->Frames();
			nc = b->Channels(); 
			
			if (x->count > 0) 
			{
				int m = min(n, x->count);
				
				x->count -= m;
				while (m--) 
				{
					f = x->fposition;
					index = (long)f;
					x->fposition += x->increment ;
					
					{
						if (f < 0)
							f = 0;
						else if (f >= frames)
							f = frames - 1;
						if (nc > 1)
							f *=  nc ;
						
						{
							if (!interpol)
							#if PD_ARRAY_DOUBLE_PRECISION
								sample = tab[2 * long(f)];
							#else
								sample = tab[long(f)];
							#endif
							else if(interpol == 1)
								sample = linear_interpol (tab,f);
							else if(interpol == 2)
								sample = square_interpol (tab,f);
							else
								sample = cubic_interpol (tab,f);
						}
					}
					
					/* attack-release stuff */
					if (index < x->attack)
					{
						w_f_index = (float)(index - x->begin) * x->attack_ratio;
					}
					else if (index > x->release)
					{
						w_f_index = (float)( x->end - index)* x->release_ratio;
					}
					else if (index < x->decay)
					{
						w_f_index = 1. + (float)(index - x->attack) * x->decay_ratio;
					}
					else w_f_index = x->sustain;
					
					sample *= x->amplitude * w_f_index;
					
					if(x->fade_out_time) //for clean voice_stealing
					{	
						if(!x->fade_out_end)
							x->fade_out_end = (index + x->fade_out_time);
						sample *= (float)(x->fade_out_end - index) / (float) x->fade_out_time;
					}
					
					*out1++ += sample;
				}
				return (1);
			}
			else
				zero:
				return (0);
		}
		
			
		/*
		 * samplor_run_all runs all voices and removes the finished voices
		 * from the used list and puts them into the free list
		 */
		void samplor_run_all(t_sample **outs, long n,long num_outputs)
		{
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			t_sample *windows = (t_sample *)this->windows;
	//		t_list_item *waitingnote;
			
			while (curr != LIST_END) 
			{
				if (samplor_run_one(curr, outs, n, windows, num_outputs, this->interpol,this->loop_xfade))
				{ /* next voice */
					prev = curr;
					curr = curr->next;
				}
				else
				{/* "removing one" */
					this->active_voices--;
					if(curr == prev)/*for the first time */
					{/* "in front" */
						this->list.used = curr->next;
						curr->next = this->list.free;
						this->list.free = curr;
						prev = curr = this->list.used;
					}
					else
					{
						curr = samplist_free_voice(&this->list,prev,curr);
					}
					if(this->voice_stealing) /* if some notes are waiting in queue ...*/
					{
		//				if(waitingnote = list_pop(&this->waiting_notes))
		//				{
		//					samplist_insert(&(this->list),0,waitingnote->inputs);
		//				}
					}
				}
			}
		}
		
		void samplor_run_all_stereo( t_sample **outs, long n,long num_outputs)
		{
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			t_sample *windows = (t_sample *)this->windows;
	//		t_list_item *waitingnote;
			
			while (curr != LIST_END) 
			{
				if (samplor_run_one_stereo(curr, outs, n, windows, num_outputs, this->interpol,this->loop_xfade))
				{ /* next voice */
					prev = curr;
					curr = curr->next;
				}
				else
				{/* "removing one" */
					this->active_voices--;

					if(curr == prev)/*for the first time */
					{/* "in front" */
						this->list.used = curr->next;
						curr->next = this->list.free;
						this->list.free = curr;
						prev = curr = this->list.used;
					}
					else
					{
						curr = samplist_free_voice(&this->list,prev,curr);
					}
			//		if(this->voice_stealing) /* if some notes are waiting in queue ...*/
			//		{
			//			if(waitingnote = list_pop(&this->waiting_notes))
			//			{
			//				samplist_insert(&(this->list),0,waitingnote->inputs);
			//			}
			//		}
				}
			}
		}
		
		/* lite version : mono, no delay, no windows */
		void samplor_run_all_lite( t_sample **outs, long n,long num_outputs)
		{
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			t_list_item *waitingnote;
			
			while (curr != LIST_END) 
			{
				if (samplor_run_one_lite(curr, outs, n, this->interpol))
				{ /* next voice */
					prev = curr;
					curr = curr->next;
				}
				else
				{/* "removing one" */
					this->active_voices--;
					if(curr == prev)/*for the first time */
					{/* "in front" */
						this->list.used = curr->next;
						curr->next = this->list.free;
						this->list.free = curr;
						prev = curr = this->list.used;
					}
					else
					{
						curr = samplist_free_voice(&this->list,prev,curr);
					}
			//		if(this->voice_stealing) /* if some notes are waiting in queue ...*/
			//		{
			//			if(waitingnote = list_pop(&this->waiting_notes))
			//			{
			//				samplist_insert(&(this->list),0,waitingnote->inputs);
			//			}
			//		}
				}
			}
		}
		
		
		/*
		 * samplor_maxvoices sets the maximum number of voices
		 */
		void samplor_maxvoices(long v)
		{
			if (v > MAX_VOICES) {
				post("samplor~: maxvoices out of range (%d), set to %d", v, MAX_VOICES);
				v = MAX_VOICES;
			}
			if (v < 1)
				v = 1;
			this->list.maxvoices = 2 * v;
			this->polyphony = v;
			samplist_init(&(this->list));
		}
		
		/*
		 * type d'interpolation
		 */
		void samplor_interpol(int interpol)
		{    
			this->interpol = interpol;
		}
		
		/*
		 * temps de cross fade pour boucle (in samples)
		 */
		void samplor_loopxfade(int loop_xfade)
		{    
			this->loop_xfade = loop_xfade;
		}
		
		/*
		 * voice_stealing
		 * 0 = no voice stealing, else a time in samples to do the fade out
		 */
		void samplor_voice_stealing(int mode)
		{    
			this->voice_stealing = mode;
		}
		
		/*
		 * samplor_start triggers a voice at p seconds after next block begin
		 */
		void samplor_start(float p)
		{
			long start;
			
			start = p * 0.001 * this->params.sr;
				
			if(this->debug)
			{
			#if 1 //DEBUG		
			post ("START_ %d %d %d %d",start,this->inputs.buf,this->inputs.offset,this->inputs.buf->Frames()/this->inputs.buf->Channels());
				buffer *b = this->inputs.buf;
			post ("%d %d %d",sizeof(t_sample),sizeof(double),sizeof(b->Data()));
			//float *tab,w_f_index; 
			float *tab,w_f_index;
			float f;
			long index, frames, nc;
			
			if (!b)
				post("oups");
			if (!b->Valid())
				post("oups");
			tab = &((*b)[0]);
			post("addresses : %d %d %d",&((*b)[0]),&((*b)[1]),tab);
			frames = b->Frames();
			nc = b->Channels(); 
			for (int i=0;i<8;i++)
				//post("%d %x %llf",i,tab[i*2],tab[i*2]);
				post("%d %f %f",i,(*b)[i],tab[i*2]);
			#endif 
			}	
			/* n'alloue pas de voix si offset > durée du son */
			if(this->inputs.buf)
			{
#if SAMPLORMAX
				if (this->inputs.buf->b_msr * this->inputs.offset < this->inputs.buf->b_size)
#else
					if ( this->inputs.offset < this->inputs.buf->Frames()/this->inputs.buf->Channels())
#endif
					if (samplist_insert(&(this->list),start,this->inputs) == 0)
						if(this->voice_stealing)
						{	/*clean voice stealing*/
							samplor_urgent_stop(this->voice_stealing);
							list_append(&this->waiting_notes,this->inputs);
						}
			}
		}
		
		/*
		 * samplor_bang triggers a grain at next block begin
		 */
		void samplor_bang()
		{
			samplor_start(0.);
		}
		
		void samplor_int(long d)
		{
		}
		
		/*
		 * samplor_set set the sound buffer
		 */
		void samplor_set( t_symbol *s)
		{
			buffer *b;
			this->inputs.buf_name = s;
			b = new buffer (this->inputs.buf_name);
			if(b->Ok()) 
			{
				this->inputs.buf = b;
				
				if(this->debug)
				{	
#if SAMPLOR_MAX
					post("sustain loop %d %d", this->inputs.buf->b_susloopstart, this->inputs.buf->b_susloopend);		// in samples
					post("release loop %d %d", this->inputs.buf->b_relloopstart,this->inputs.buf->b_relloopend);		// in samples
					
#endif	
					post("nframes  %d size %d %d", this->inputs.buf->Frames(),(int)((this->inputs.buf->Frames())/44.1),this->inputs.buf->Channels() );		// in samples

				}
				
				
			} else {
				
					post("%s (%s) - warning: buffer is currently not valid!",thisName(),GetString(thisTag()));
				error("samplor~: no buffer~ %s", s->s_name);
				this->inputs.buf = 0;
			}
		}
		
		/*
		 * samplor_buf set the sound buffer
		 */
		void samplor_buf(int buf)
		{    
			char bufname[10] = "sample100";
			buf = min(buf,999);
			//	sprintf(bufname,"%s%d",this->inputs.bufprefix,buf);
			snprintf(bufname,sizeof(bufname),"sample%d",buf);
			this->inputs.samplenumber = buf;
			samplor_set(gensym(bufname));
		}
		
		/*
		 * samplor_name set the sound buffer with the full name
		 */
		void samplor_bufname(char *name)
		{    
			char *bufname = "sample100";
			bufname = name;
			//	this->inputs.samplenumber = 1; /* avec cette methode, stop method marche pas correctement!*/
			this->inputs.samplenumber = -1; 
			samplor_set(gensym(bufname));
		}
		
		/*
		 * samplor_offset
		 */
		void samplor_offset(int offset)
		{    
			if (offset < 0)
				offset = 0;
			this->inputs.offset = offset;
		}
		
		/*
		 * samplor_dur set the duration 
		 */
		void samplor_dur(int dur)
		{    
			this->inputs.dur = dur;
		}
		
		/*
		 * samplor_transp
		 */
		void samplor_transp (double transp)
		{    
			this->inputs.transp = transp;
		}
		
		/*
		 * samplor_amp
		 */
		void samplor_amp( double amp)
		{ 
			this->inputs.amp = amp;
		}
		
		/*
		 * samplor_pan
		 */
		void samplor_pan(double pan)
		{    
			int chan;
			
			this->inputs.pan = pan;
			
			if (this->num_outputs > 2)
			{
				chan = (int)pan;
				this->inputs.pan -=  chan;
				if ((chan < 0) || (chan > (this->num_outputs -1)))
					chan = 0;
				this->inputs.chan = chan;
				this->inputs.chan2 = (chan + 1) % this->num_outputs;
			}
			
			//	post ("pan %f %d",this->inputs.pan,this->inputs.chan);
		}
		
		/*
		 * samplor_rev
		 */
		void samplor_rev(double rev)
		{    
			this->inputs.rev = rev;
		}
		
		/*
		 * samplor_win
		 */
		void samplor_win(int win)
		{    
			if ((win < 0) || (win > NUM_WINS))
				win = 0;
			this->inputs.env = win;
		}
		
		/*
		 * samplor_winar : window + atack and decay
		 */
		void samplor_winar(int win,int attack,int release)
		{    
			if ((win < 0) || (win > NUM_WINS))
				win = 0;
			this->inputs.env = win;
			this->inputs.attack = attack;
			this->inputs.release = release;
		}
		
		/*
		 * samplor_attack_release
		 */
		void samplor_attack(int dur)
		{  if (dur < 0)
			dur = 0;
			this->inputs.attack = dur;
		}
		void samplor_decay(int dur)
		{  if (dur < 0)
			dur = 0;
			this->inputs.decay = dur;
		}
		void samplor_sustain(int val)
		{  if (val < 0)
			val = 0;
			this->inputs.sustain = val;
		}
		
		void samplor_release(int dur)
		{
			if (dur < 0)
				dur = 0;
			this->inputs.release = dur;
		}
		void samplor_adsr_assign(const t_symbol *s, int ac,const t_atom *argv)
		{
			samplor_win(0);
			if (ac > 0) samplor_attack(GetAInt(argv[0]));
			if (ac > 1) samplor_decay(GetAInt(argv[1]));
			if (ac > 2) samplor_sustain(GetAInt(argv[2]));
			if (ac > 3) samplor_release(GetAInt(argv[3]));
		}
		
		void samplor_adsr(const t_symbol *s, int ac,const t_atom *argv)
		{
			this->inputs.adsr_mode = 1;
			samplor_adsr_assign(s, ac, argv);
		}

		/*
		 * to stop one sample (arret rapide)
		 */
		void samplor_stop2(long time)
		{	samplor_urgent_stop(time);}
		
		/*
		 * to stop all samples (arret rapide)
		 */
		void samplor_stopall(long time)
		{	  
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			
			time *= this->params.sr / 1000.;
			while (curr != LIST_END) 
			{
				if(curr->loop_flag)
					curr->loop_flag = 0;
				{    
					//	if (time > 0)
					{curr->fade_out_time = time * curr->increment;
						curr->count = time;
					}
					/*				else
					 {
					 curr->loop_flag = 0;
					 curr->fade_out_time = curr->end - curr->release;
					 curr->count = curr->fade_out_time / curr->increment;
					 }
					 */
					//				post ("stop voice %d",curr->count);
				}		   	
				prev = curr;
				curr = curr->next;
			}
		}
		
		void samplor_urgent_stop(long time)
		{
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			if (curr)
			{
				time *= this->params.sr / 1000.;
				curr->loop_flag = 0;
				curr->count = time / curr->increment ;
				curr->fade_out_time = time;
			}
		}
		
		/*
		 * to stop a looped sample (sort de la boucle)
		 */
		void samplor_stop(const t_symbol *s,short ac, t_atom *av)
		{	
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			int sample;
			
			switch (ac)
			{case 0 :
				{
					while (curr != LIST_END) 
					{
						curr->loop_flag = 0;
						prev = curr;
						curr = curr->next;
					}
					break;
				}
				case 1:
				{
					sample = (int)GetAInt(av[0]);
					while (curr != LIST_END) 
					{
						if(curr->samplenumber == sample)
							curr->loop_flag = 0;
						prev = curr;
						curr = curr->next;
					}
					break;
				}
				case 2:
				{samplor_stop_one_voice((int)GetAInt(av[0]),GetAInt(av[1]));
				}
			}
		}
		
		void samplor_stop_one_voice(int sample,float transp)
		{
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			long time;
			
			while (curr != LIST_END) 
			{
				if((curr->samplenumber == sample)&&(curr->increment==transp))
				{
					if(curr->loop_flag)
						curr->loop_flag = 0;
					//			if(x->ctlp->debug)
					//				post ("stop < %d",curr->count);
					time = curr->end - curr->release;
					if (curr->count > time) //to avoid stopping notes already stopped but not yet completely gone ...
					{
						curr->fade_out_time = time * transp; /* correct fade out time */
						curr->count = time;
						break;
					}
				}
				prev = curr;
				curr = curr->next;
			}
		}
		
		void samplor_stop_one_voice_str(t_symbol *buf_name,float transp)
		{
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			long time;
			
			while (curr != LIST_END) 
			{
				if((!strcmp(buf_name->s_name,curr->buf->Name()))&&((curr->increment==transp)||(transp==0))&&(curr->loop_flag!=FINISHING_LOOP))
						//arrete tous les echantillons de ce nom si transp = 0 (version 2.02)
				{
					time = curr->end - curr->release;
                    if(curr->loop_flag)
                    {
                        curr->loop_flag = FINISHING_LOOP;
                        curr->fade_out_time = time * transp; /*  ???*/
                        curr->count = time;
                        
                        break;
                    }

					
					if (curr->count > time) //to avoid stopping notes already stopped but not yet completely gone ...
					{
						curr->fade_out_time = time * curr->increment; /* correct fade out time */
						curr->count = time;
						
                        break;
					}
				}
				prev = curr;
				curr = curr->next;
			}
		}
			
		/*
		 * to modify loop points
		 */
		void samplor_loop(const t_symbol *s,short ac, t_atom *av)
		{	
			t_samplor_entry *prev = this->list.used;
			t_samplor_entry *curr = prev;
			int sample;
			float susloopstart, susloopend;
			if (ac > 0) susloopstart = GetAInt(av[0]);
			if (ac > 1) susloopend = GetAInt(av[1]);
			if (ac > 2) 
			{
				sample = GetAInt(av[2]);
				while (curr != LIST_END) 
				{
					if(curr->samplenumber == sample)
					{curr->loop_beg_d = susloopstart;
						curr->loop_end_d = susloopend;
						curr->loop_dur_d = susloopend - susloopstart;
					break;}
					prev = curr;
					curr = curr->next;
				}
			}
			else while (curr != LIST_END) 
			{
				curr->loop_beg_d = susloopstart;
				curr->loop_end_d = susloopend;
				curr->loop_dur_d = susloopend - susloopstart;
				prev = curr;
				curr = curr->next;
			}
		}
        void samplor_set_buffer_loop(const t_symbol *s,short ac, t_atom *av)
        {
            long loopstart = 0,loopend = 0;
            const char * buf_name = GetAString(av[0]);
            
			if (ac > 1) loopstart = GetAInt(av[1]);
			if (ac > 2) loopend = GetAInt(av[2]);
            
            loopMap[buf_name]=std::make_pair<long,long>(loopstart,loopend);
        }

        void samplor_get_buffer_loop(const t_symbol *s,short ac, t_atom *av)
        {
            const char * buf_name = GetAString(av[0]);
            std::pair<long,long> result;
          /*
           std::tr1::unordered_map<std::string,std::pair<long,long> >::const_iterator got = loopMap.find (buf_name);
            
            if ( got == loopMap.end() )
                post("not found");
            else
                post("%s is %d %d\n",got->first.c_str(),got->second.first,got->second.second);
           */
            
            result = get_buffer_loop(buf_name);
            post("!! %s is %d %d\n",buf_name,result.first,result.second);
        }
     	
		/*
		 * samplor_list triggers one voice with : (delay, sample, offset, duration, ...) 
		 */
		void samplor_list(const t_symbol *s,short ac, t_atom *av)
		{
			float amp;
			
			if (ac > 1) 
			{
				if (IsSymbol(av[1]))
					samplor_bufname(GetSymbol(av[1])->s_name);
				else 
					{samplor_buf(GetInt(av[1]));}
			}
			if (ac > 2) samplor_offset(GetInt(av[2]));
			if (ac > 3) samplor_dur(GetInt(av[3]));
			if (ac > 4) samplor_transp(GetFloat(av[4]));
			if (ac > 5)  // linear amplitude 
			{
				if ((amp = GetFloat(av[5])) > 0.)
					samplor_amp(amp);
				else 	// noteoff
				{
					if (this->inputs.samplenumber == -1)
						samplor_stop_one_voice_str(this->inputs.buf_name,this->inputs.transp);
					else
						samplor_stop_one_voice(this->inputs.samplenumber,this->inputs.transp);
					return;
				}
			}
			if (ac > 6)
			{
				if(IsFloat (av[6]))
					samplor_pan(GetFloat(av[6]));
				else if (IsInt (av[6]))
					samplor_pan(GetInt(av[6]));
			}			
			if (ac > 7) samplor_rev(GetFloat(av[7]));
			samplor_start(GetInt(av[0]));
		}
	
		void samplor_performN(int n,t_sample *const *invecs,t_sample *const *outvecs)
		{
			long i =  n;
			long j;
			
			this->vectors[0] = outvecs[0];
			for (j=0;j<(this->num_outputs);j++)	
				this->vectors[j] = outvecs[j];
			
			while(i--)
			{
				for (j=0;j<(num_outputs);j++)
					this->vectors[j][i] = 0.;
			}
			samplor_run_all(this->vectors, n, this->num_outputs);	
		}
		
		
		void samplor_perform3(int n,t_sample *const *invecs,t_sample *const *outvecs)
		{
			t_sample *out1 = outvecs[0];
			t_sample *out2 = outvecs[1];			
			t_sample *out3 = outvecs[2];
			t_sample *outs[3];
			long i =  n;
			float *o1 = out1;
			float *o2 = out2;
			float *o3 = out3;
			outs[0] = out1;
			outs[1] = out2;
			outs[2] = out3;
			
			while(i--)
			{
				*o1++ = 0.;
				*o2++ = 0.;
				*o3++ = 0.;
			}
			samplor_run_all(outs, n, 3);	
		}
		
		void samplor_perform2(int n,t_sample *const *invecs,t_sample *const *outvecs)
		{
			t_sample *out1 = outvecs[0];
			t_sample *out2 = outvecs[1];
			t_sample *outs[2];
			
			long i = n;
			float *o1 = out1;
			float *o2 = out2;
			outs[0] = out1;
			outs[1] = out2;
			
			while(i--)
			{
				*o1++ = 0.;
				*o2++ = 0.;
			}
			samplor_run_all(outs, n, 2);	
		}
		
		void samplor_perform_stereo(int n,t_sample *const *invecs,t_sample *const *outvecs)
		{
			t_sample *out1 = outvecs[0];
			t_sample *out2 = outvecs[1];
			t_sample *outs[2];
			
			long i = n;
			float *o1 = out1;
			float *o2 = out2;
			outs[0] = out1;
			outs[1] = out2;
			
			while(i--)
			{
				*o1++ = 0.;
				*o2++ = 0.;
			}
			samplor_run_all_stereo( outs, n, 2);	
			
		}
		
		
		void samplor_perform1(int n,t_sample *const *invecs,t_sample *const *outvecs)
		{
			t_sample *out1 = outvecs[0];
			t_sample *outs[1];
			
			long i = n;
			float *o = out1;
			outs[0] = out1;
			
			while(i--)
				*o++ = 0.;
			samplor_run_all( outs, n, 1);	
			
		}
		

		void samplor_perform0(int n,t_sample *const *invecs,t_sample *const *outvecs)
		{
			t_sample *out1 = outvecs[0];
			t_sample *outs[1];
			
			long i = n;
			float *o = out1;
			outs[0] = out1;
			
			while(i--)
				*o++ = 0.;
			samplor_run_all_lite(outs, n, 1);	
			
		}

		
#if 0	
		void samplor_dsp(t_sigsamplor *x, t_signal **sp, short *count)
		{  
			int i;
			this->params.sr = sys_getsr();
			this->params.vs = sp[0]->s_n;
			if (x->num_outputs > 3)
			{
				
				x->vectors[0] = sp[0]->s_vec;
				for (i=0;i<(x->num_outputs);i++)	
					x->vectors[i] = sp[i]->s_vec;
				
				dsp_add(samplor_performN, 2, x, sp[0]->s_n);
			}
			else if (x->num_outputs == 3)
				dsp_add(samplor_perform3, 5, this, sp[0]->s_vec, sp[1]->s_vec,sp[2]->s_vec, sp[0]->s_n);
			else if (x->num_outputs == 2)
				if (x->stereo_mode == 1)
					dsp_add(samplor_perform_stereo, 4, this, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
				else
					dsp_add(samplor_perform2, 4, this, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
				else if (x->num_outputs == 1)
					dsp_add(samplor_perform1, 3, this, sp[0]->s_vec, sp[0]->s_n);
				else
					dsp_add(samplor_perform0, 3, this, sp[0]->s_vec, sp[0]->s_n);	
		}
#endif	
		void samplor_manual_init(long d)
		{
			int i;
			post("init level %d",d);
			if (d >= 4)
				//	this->ctlp = &(this->ctl);
				if(d>=3)
					samplor_init();
			if(d>=2)
				samplor_maxvoices(12);	
			if (d>=1)
			{
				this->time = 0;
				this->count = 0;
				
				//	x->x_obj.z_misc = Z_NO_INPLACE;
			}
			for (i=0;i<=this->num_outputs;i++)
				this->vectors[i] = NULL;
		}
		
		
		void samplor_debug(long d)
		{
			this->debug = d;
			samplist_display(&(this->list));
			samplist_count(&(this->list));
/*			if (d == 2)
			{
				t_samplor_entry *prev = this->list.used;
				t_samplor_entry *curr = prev;
				while (curr != LIST_END) 
				{
					if(curr == prev)
						this->list.used = curr->next;
					prev->next = curr->next;
					curr->next = this->list.free;
					this->list.free = curr;
					curr = prev->next;
				}
			}
			else
			{
				t_samplor_entry *prev = this->list.used;
				t_samplor_entry *curr = prev;
				
				while (curr != LIST_END) 
				{
					post("list %d",curr->samplenumber);
					{ // next voice 
						prev = curr;
						curr = curr->next;
					}
				}
			}
*/	
		}
		
		static void setup(t_classid c)
		{
			//register methods 
			post("%s",VERSION);
            post("compiled %s %s",__DATE__, __TIME__);
            
			FLEXT_CADDMETHOD_(c,0,"maxvoices",samplor_maxvoices);
			FLEXT_CADDMETHOD(c,0,samplor_int);
			FLEXT_CADDMETHOD_(c,0,"set",samplor_set);
			FLEXT_CADDMETHOD_I(c,0,"debug",samplor_debug);
			FLEXT_CADDMETHOD_I(c,0,"init",samplor_manual_init);
			FLEXT_CADDMETHOD_I(c,0,"interpol",samplor_interpol);
			FLEXT_CADDMETHOD_I(c,0,"xfade",samplor_loopxfade);
			FLEXT_CADDMETHOD_I(c,0,"voice_stealing",samplor_voice_stealing);
			FLEXT_CADDMETHOD_(c,0,"list",samplor_list);
			FLEXT_CADDMETHOD_I(c,0,"window",samplor_win);
			FLEXT_CADDMETHOD_(c,0,"adsr",samplor_adsr);
			FLEXT_CADDMETHOD_(c,0,"stop",samplor_stop);
			FLEXT_CADDMETHOD_I(c,0,"stop2",samplor_stop2);
			FLEXT_CADDMETHOD_I(c,0,"stopall",samplor_stopall);
			FLEXT_CADDMETHOD_(c,0,"loop",samplor_loop);
            FLEXT_CADDMETHOD_(c,0,"buffer_loop",samplor_set_buffer_loop);
            FLEXT_CADDMETHOD_(c,0,"get_buffer_loop",samplor_get_buffer_loop);
		
			FLEXT_CADDBANG(c,0,samplor_bang);
			FLEXT_CADDMETHOD(c,0,samplor_start);
			FLEXT_CADDMETHOD(c,1,samplor_buf);
			FLEXT_CADDMETHOD(c,2,samplor_offset);
			FLEXT_CADDMETHOD(c,3,samplor_dur);
			FLEXT_CADDMETHOD(c,4,samplor_transp);
			FLEXT_CADDMETHOD(c,5,samplor_amp);
			FLEXT_CADDMETHOD(c,6,samplor_pan);
			FLEXT_CADDMETHOD(c,7,samplor_rev);
		}
		// for every registered method a callback has to be declared
		FLEXT_CALLBACK_I(samplor_maxvoices)
		FLEXT_CALLBACK_I(samplor_int)
		FLEXT_CALLBACK_S(samplor_set)	
		FLEXT_CALLBACK_I(samplor_debug)
		FLEXT_CALLBACK_I(samplor_manual_init)
		FLEXT_CALLBACK_I(samplor_interpol)
		FLEXT_CALLBACK_I(samplor_loopxfade)
		FLEXT_CALLBACK_I(samplor_voice_stealing)
		FLEXT_CALLBACK_A(samplor_list)
		FLEXT_CALLBACK_A(samplor_adsr)
		FLEXT_CALLBACK_A(samplor_stop)
		FLEXT_CALLBACK_I(samplor_stop2)
		FLEXT_CALLBACK_I(samplor_stopall)
		FLEXT_CALLBACK_A(samplor_loop)
        FLEXT_CALLBACK_A(samplor_set_buffer_loop)
        FLEXT_CALLBACK_A(samplor_get_buffer_loop)
		FLEXT_CALLBACK(samplor_bang)
		FLEXT_CALLBACK_F(samplor_start)
		FLEXT_CALLBACK_I(samplor_buf)
		FLEXT_CALLBACK_I(samplor_offset)
		FLEXT_CALLBACK_I(samplor_dur)
		FLEXT_CALLBACK_F(samplor_transp)
		FLEXT_CALLBACK_F(samplor_amp)
		FLEXT_CALLBACK_I(samplor_win)
		FLEXT_CALLBACK_F(samplor_pan)
		FLEXT_CALLBACK_F(samplor_rev)		
		
	}; // end of class declaration for samplor2

// Before we can run our signal1-class in PD, the object has to be registered as a
// PD object. Otherwise it would be a simple C++-class, and what good would
// that be for?  Registering is made easy with the FLEXT_NEW_* macros defined
// in flext.h. For tilde objects without arguments call:

FLEXT_NEW_DSP_2("samplor3~ samplor2~", samplor2,int,int)

// instantiate the class
// Now we define our DSP function. It gets this arguments:
// 
// int n: length of signal vector. Loop over this for your signal processing.
// float *const *in, float *const *out: 
//          These are arrays of the signals in the objects signal inlets rsp.
//          oulets. We come to that later inside the function.

bool samplor2::CbDsp()
{
	// this is hopefully called at each dsp chain build
	
	// post("%s - DSP reset!",thisName());
	
    // for PD at least this is also called if a table has been deleted...
    // then we must reset the buffer
	
	SETSIGFUN(samplordspfun,SIGFUN(samplor_perform0));
	
	
	switch(num_outputs){
		case 3 : 
			SETSIGFUN(samplordspfun,SIGFUN(samplor_perform3));
			break;			
		case 2 : 
			if(this->stereo_mode)
			{
				SETSIGFUN(samplordspfun,SIGFUN(samplor_perform_stereo));
			}
			else
			{
				SETSIGFUN(samplordspfun,SIGFUN(samplor_perform2));
			}
			break;
			
			case 1 : 
			SETSIGFUN(samplordspfun,SIGFUN(samplor_perform1));
			break;
			case 0 : SETSIGFUN(samplordspfun,SIGFUN(samplor_perform0));
			break;
			default : SETSIGFUN(samplordspfun,SIGFUN(samplor_performN));
			break;
	}
    return true;
}

void samplor2::CbSignal() 
{ 
	//ZeroSamples(OutSig(),Blocksize());
	samplordspfun(Blocksize(),InSig(),OutSig()); 
}
#if 0 //DEPRECATED replaced by CbSignal !!
void samplor2::m_signal(int n, float *const *in, float *const *out)
{
	
	float *outs          = out[0];
	// outs holds the signal vector at the one signal outlet we have.
	t_sample *outss[1];
	
    long i = n;
    float *o = outs;
	outss[0] = outs;

 	
    while(i--)
		*o++ = 0.;
	samplor_run_all_lite(outss, n, 1);	
	
	
}  // end m_signal

#endif


// constructor method
samplor2::samplor2( int numoutputs,int maxvoices)
{
	int i,n;
	
	if (numoutputs == -2)
	{
		this->stereo_mode = 1;
		numoutputs = 2;
	}
	else 
		this->stereo_mode = 0;
	
	numoutputs = min(numoutputs,16);
	n = max(numoutputs,1);
	this->num_outputs = max(numoutputs,0);
	
	/* INLETS */
	AddInAnything("message inlet"); // add one inlet for any message
	//		AddInInt("int1 ?");    
	AddInInt("int2");    
	AddInInt("int3");    
	AddInFloat("float 4");    // 1 float in
	AddInFloat("float 5");    // 1 float in
	if (this->num_outputs >= 2)
		AddInFloat("float 6 ?");    // 1 float in
	if (this->num_outputs == 3)
		AddInFloat("float 7 ?");    // 1 float in
	
	/* OUTLETS */
	while(n--)
		AddOutSignal("audio out");          // n audio outputs 
	
	samplor_init();
	samplor_maxvoices(maxvoices);	
	time = 0;
    count = 0;
	active_voices = 0;
	
	for (i=0;i <= this->num_outputs;i++)
		this->vectors[i] = NULL;
	
	
} // end of constructor




