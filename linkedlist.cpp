/*
 ******************
 LINKED LIST
 ********************
 */

#include <string>
#include <map>
#include <tr1/unordered_map>
#include <flext.h>
#include "samplor2.h"

extern std::tr1::unordered_map <std::string , std::pair<long,long> > loopMap ; //because no such thing as loop support in puredata "buffers"

/*VOICES*/

/*
 * samplor_init_list initializes the free list 
 */
void samplist_init(t_samplor_list *x)
{
    t_samplor_entry *p = x->samplors;
    int i;
    
    x->free = p;
    x->used = (t_samplor_entry*) LIST_END;
	x->at_end = (t_samplor_entry*) LIST_END;
    for (i = 1; i < x->maxvoices; i++)
		p[i - 1].next = &p[i];
    p[--i].next = (t_samplor_entry*) LIST_END;
}

/* libere une voix */

t_samplor_entry *samplist_free_voice(t_samplor_list *x, t_samplor_entry *prev, t_samplor_entry *curr)
{
	prev->next = curr->next;
    curr->next = x->free;
    x->free = curr;
    return(prev->next);
}

t_samplor_entry *samplist_pop(t_samplor_list *x)
{
	t_samplor_entry *prev = (t_samplor_entry *)&(x->used);
    t_samplor_entry *curr = prev->next;
    t_samplor_entry *val = prev->next;
    
	if	(x->used == LIST_END)return 0;
	else
	{
		prev->next = curr->next;
    	curr->next = x->free;
    	x->free = curr;
    	return(val);
    }
}

t_samplor_entry *samplist_new_voice(t_samplor_list *x, long start, t_samplor_inputs inputs)
{
  	int attack_dur,decay_dur,release_dur;
  	long dur,bufsize;
    std::pair<long,long> suslooppoints(0,0);
	
	t_samplor_entry *niou = x->free;
	
	if(niou == (t_samplor_entry *)LIST_END) return ((t_samplor_entry *) 0);
	else
	{
		x->free = niou->next;	
		
		niou->buf = inputs.buf;
		niou->samplenumber = inputs.samplenumber;
	//	niou->buf = inputs.buf;
		niou->start = start;
#if SAMPLORMAX
		niou->position = inputs.offset * niou->buf->b_msr;
#else
		niou->position = inputs.offset * 44.1;
#endif
		if (inputs.dur < 0)
		{
			/* loop : */
			if (inputs.dur == -2)
				niou->loop_flag = ALLER_RETOUR_LOOP;
			else
				niou->loop_flag = LOOP;
#if SAMPLORMAX
			if (inputs.buf->b_susloopstart && inputs.buf->b_susloopend)
			{	//loop points in aiff
				niou->loop_beg = niou->loop_beg_d = inputs.buf->b_susloopstart;
				niou->loop_end = niou->loop_end_d = inputs.buf->b_susloopend;
			}

#else
            suslooppoints = get_buffer_loop(inputs.buf->Name());
                if (suslooppoints.second)
                {
	//loop points in loopMax map
                    niou->loop_beg = suslooppoints.first;
                    niou->loop_end = suslooppoints.second;
                }
                
#endif
            else
                
			{	// si pas de loop dans le fichier : prendre tout le son
				//niou->loop_beg = niou->loop_beg_d = 0;
				//niou->loop_end = niou->loop_end_d = inputs.buf->Frames()  - 256;
               // si pas de loop dans le fichier : ne pas boucler
                niou->loop_flag = 0;
			}
			// post ("%d %d loop",niou->loop_beg,niou->loop_end);
			niou->loop_dur = niou->loop_dur_d = niou->loop_end - niou->loop_beg;
			niou->dur = inputs.buf->Frames();
		}
		else
		{
			/* no loop : */
			if (inputs.dur == 0)
			{
				niou->loop_flag = IGNORE_LOOP;
				dur = inputs.buf->Frames(); // joue tout le son
			}
			else
			{
				niou->loop_flag = 0;
#if SAMPLORMAX
				dur = (long)(inputs.dur * niou->buf->b_msr);
#else
				dur = (long)(inputs.dur * 44.1);
				// post("DEBUG POS %d DUR %d F %d F %d",niou->position,dur,niou->buf->Frames(),(int)((float)niou->buf->Frames()/(44.1 * (float)niou->buf->Channels())));
#endif
			}
#if SAMPLORMAX
			if ((niou->position + dur) > niou->buf->b_size)
			/* corrige la durée si elle dépasse la fin du son */
				dur = niou->buf->b_size - niou->position;
			niou->dur = dur;
			dur = (long) (dur / niou->buf->b_msr);
			
#else
			bufsize = niou->buf->Frames() / niou->buf->Channels(); // buffer size
			if ((niou->position + dur) > bufsize)
			/* corrige la durée si elle dépasse la fin du son */
				dur = bufsize - niou->position;
			niou->dur = dur;
			dur = (long) (dur / 44.1);
			
#endif
			if ((inputs.attack + inputs.decay + inputs.release) > dur)
			{/* adsr correction */
				inputs.attack =  0.25 * dur;
				inputs.decay =  inputs.attack;
				inputs.release = inputs.attack;
			}
		}
		niou->fposition = niou->position;
		niou->fposition2 = niou->position;
		niou->begin = niou->position;
		niou->end = niou->begin + niou->dur ;
		niou->count = niou->dur / inputs.transp ;
		//	new->count = abs (new->count) ; //modification de version 75 pour pouvoir jouer des sons à l'envers !?
		niou->increment = inputs.transp;
		niou->amplitude = inputs.amp;
		niou->pan = inputs.pan;
		niou->chan = inputs.chan;
		niou->chan2 = inputs.chan2;
		niou->rev = inputs.rev;
		
		/* envelope : */
		niou->win = inputs.env;
		niou->sustain = inputs.sustain * 0.01;
		niou->fade_out_time = 0;
		niou->fade_out_end = 0;
#if SAMPLORMAX		
		attack_dur = inputs.attack * niou->buf->b_msr;
		decay_dur = inputs.decay * niou->buf->b_msr;
		release_dur = inputs.release * niou->buf->b_msr;
#else
		attack_dur = inputs.attack * 44.1;
		decay_dur = inputs.decay * 44.1;
		release_dur = inputs.release * 44.1;
#endif
		niou->attack =  niou->begin + attack_dur;
		niou->decay =  niou->attack + decay_dur;
		niou->release = niou->end - release_dur;
		niou->attack_ratio = 1./(float)attack_dur;
		niou->decay_ratio = (niou->sustain - 1.)/(float)decay_dur;
		niou->release_ratio = niou->sustain/(float)release_dur;
		
#if DEBUG	 	
	 	post ("win st dur %d %d %d",niou->win,niou->start,niou->dur);
	 	post ("pos inc amp count %f %f %f %d",niou->fposition,niou->increment,niou->amplitude,niou->count);
	 	post ("pan attack decay sustain release %f %d %d %f %d",niou->pan,niou->attack,niou->decay,niou->sustain,niou->release);
		post ("loop %d %d %d",niou->loop_flag,niou->loop_end,niou->loop_dur);
#endif
		return(niou);
	}
}

t_samplor_entry *samplist_insert(t_samplor_list *x,long start, t_samplor_inputs inputs)
{
	t_samplor_entry *pt = samplist_new_voice(x,start,inputs);
	if(pt)
	{
		if(x->used == LIST_END)
			x->at_end = pt;
		pt->next = x->used;
		x->used = pt;
	}
	return(pt);
}
t_samplor_entry *samplist_append(t_samplor_list *x,long start, t_samplor_inputs inputs)
{
	t_samplor_entry *pt = samplist_new_voice(x,start,inputs);
	if(pt)
	{
		if(x->used == LIST_END)
			x->used = pt;
		else
			x->at_end->next = pt;
		pt->next = (t_samplor_entry *) LIST_END;
		x->at_end = pt;
	}
	return (pt);
}

void samplist_display(t_samplor_list *x)
{
    t_samplor_entry *pt = x->used;
 	
	while (pt != LIST_END) 
	{
		post("list %d-%d ",pt->start ,pt->samplenumber);
		pt=pt->next;
	}
}
/*print the number of active voices */
void samplist_count(t_samplor_list *x)
{
    t_samplor_entry *pt = x->used;
    int count = 0;
 	
	while (pt != LIST_END) 
	{
		count ++;
		pt=pt->next;
	}
	post("active: %d ",count);
}


/*******************
 *  WAITING NOTES  *
 *******************/

void list_init(t_list *x)
{
    t_list_item *p = x->items;
    int i;
    
    x->free = p;
    x->list = (t_list_item *) LIST_END;
	x->at_end = (t_list_item *) LIST_END;
	for (i = 1; i < WAITINGNOTES ; i++)
		p[i - 1].next = &p[i];
    p[--i].next = (t_list_item *)LIST_END;
	
}

t_list_item *new_list_item(t_list *x,LIST_TYPE inputs)
{
	t_list_item *niou = x->free;
	if(niou == LIST_END)return ((t_list_item *) 0);
	else
	{
		x->free = niou->next;
		//	niou->inputs.bufprefix = inputs.bufprefix;
		niou->inputs.buf_name = inputs.buf_name;
		niou->inputs.buf = inputs.buf ;
		niou->inputs.samplenumber = inputs.samplenumber;
		niou->inputs.offset = inputs.offset;			
		niou->inputs.dur = inputs.dur;			
		niou->inputs.attack = inputs.attack;			
		niou->inputs.decay = inputs.decay;			
		niou->inputs.sustain = inputs.sustain;			
		niou->inputs.release = inputs.release;			
		niou->inputs.transp = inputs.transp;
		niou->inputs.amp = inputs.amp;
		niou->inputs.pan = inputs.pan;
		niou->inputs.rev = inputs.rev;
		niou->inputs.env = inputs.env;
		
		return(niou);
	}
}

void list_insert(t_list *x,LIST_TYPE inputs)
{
	t_list_item *pt = new_list_item(x,inputs);
	if(pt)
	{
		if(x->list == LIST_END)
			x->at_end = pt;
		pt->next = x->list;
		x->list = pt;
	}
}

void list_append(t_list *x,LIST_TYPE inputs)
{
	t_list_item *pt = new_list_item(x,inputs);
	if(pt)
	{
		if(x->list == LIST_END)
			x->list = pt;
		else
			x->at_end->next = pt;
		pt->next = (t_list_item *)LIST_END;
		x->at_end = pt;
	}
}

t_list_item *list_pop(t_list *x)
{
	t_list_item *prev = (t_list_item*)&(x->list);
    t_list_item *curr = prev->next;
    t_list_item *val = prev->next;
    
	if	(x->list == LIST_END)return 0;
	else
	{
		prev->next = curr->next;
    	curr->next = x->free;
    	x->free = curr;
    	return(val);
    }
}

void list_display (t_list *x)
{
	t_list_item *pt = x->list;
	post("( ");
	while (pt != LIST_END)
	{
		post("%d ",pt->inputs);
		pt=pt->next;
	}
	post(")\n");
}

std::pair<long,long> get_buffer_loop (const char * buf_name)
{
    std::tr1::unordered_map<std::string,std::pair<long,long> >::const_iterator got = loopMap.find (buf_name);
   
    
    if ( got == loopMap.end() )
        return (std::pair<long,long> (0,0));
    else
    {
    //     post("get %s -> %d %d \n", buf_name,got->second.first,got->second.second);
        return(got->second);
    }
}



