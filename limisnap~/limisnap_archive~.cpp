#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object
#include "ext_common.h"
#include "z_dsp.h"

#include <memory>
#include "../limisnap~/c74_lib_dcblocker.h"
#include "../limisnap~/c74_min_operator_vector.h"
#include "../limisnap~/c74_min_dataspace.h"
#include "../limisnap~/c74_lib_limiter.h"

#ifdef MC_VERSION
#define LIMISNAP_NAME "mcs.limisnap~"
#else
#define LIMISNAP_NAME "limisnap~"
#endif


typedef struct _limisnap {
	t_pxobject				x_obj;
	c74::min::lib::limiter	*x_limiter;
	int						x_channelcount;
	int						x_buffersize;
	char					x_bypass;
	char					x_dcblock;
	char					x_mode;
	long 					x_lookahead;
	double					x_preamp;
	double					x_postamp;
	double					x_threshold;
	double					x_release;
	double* x_meters;						// meter value array
	double					x_gr;			// gain reduction value
	void					*x_out;			// gain reduction float output for display
	void					*p_clock;		//x poll timer
	char					p_startclock;	//x poll timer start flag
	int						p_pollrate;		//x poll timer update rate (ms)
											//x use qelem for low priority?
} t_limisnap;


void *limisnap_new(t_symbol *s, long argc, t_atom *argv);
void limisnap_free(t_limisnap *x);
void limisnap_assist(t_limisnap *x, void *b, long m, long a, char *s);
long limisnap_multichanneloutputs(t_limisnap *x, int index);
long limisnap_inputchanged(t_limisnap *x, long index, long chans);
void limisnap_clear(t_limisnap *x);
void limisnap_bang(t_limisnap* x);	// bang poll
void limisnap_tick(t_limisnap* x);	//x auto poll
t_max_err limisnap_setpollrate(t_limisnap* x, void* attr, long argc, t_atom* argv);
t_max_err limisnap_setbypass(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setdcblock(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setmode(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setlookahead(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setpreamp(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setpostamp(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setthreshold(t_limisnap *x, void *attr, long argc, t_atom *argv);
t_max_err limisnap_setrelease(t_limisnap *x, void *attr, long argc, t_atom *argv);
void limisnap_dsp64(t_limisnap *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);


static t_class *s_limisnap_class;
static t_symbol* ps_gr;
static t_symbol* ps_meters;

void ext_main(void *r)
{
	t_class *c = class_new(LIMISNAP_NAME, (method)limisnap_new, (method)limisnap_free, sizeof(t_limisnap), NULL, A_GIMME, 0);
		
	class_addmethod(c, (method)limisnap_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)limisnap_assist,"assist", A_CANT, 0);
#ifdef MC_VERSION
	class_addmethod(c, (method)limisnap_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
	class_addmethod(c, (method)limisnap_inputchanged, "inputchanged", A_CANT, 0);
#endif
	class_addmethod(c, (method)limisnap_clear,"clear",0);
	class_addmethod(c, (method)limisnap_bang, "bang", 0);

	CLASS_ATTR_CHAR(c, "poll", 0, t_limisnap, p_pollrate);
	CLASS_ATTR_LABEL(c, "poll", 0, "poll");
	CLASS_ATTR_ACCESSORS(c, "poll", NULL, limisnap_setpollrate);
	CLASS_ATTR_FILTER_MIN(c, "poll", 0);

	CLASS_ATTR_CHAR(c,		"bypass", 0, t_limisnap, x_bypass);
	CLASS_ATTR_LABEL(c,		"bypass", 0, "Bypass");
	CLASS_ATTR_ACCESSORS(c,	"bypass", NULL, limisnap_setbypass);
	CLASS_ATTR_STYLE(c, 	"bypass", 0, "onoff");

	CLASS_ATTR_CHAR(c,		"dcblock", 0, t_limisnap, x_dcblock);
	CLASS_ATTR_LABEL(c,		"dcblock", 0, "Block DC offsets");
	CLASS_ATTR_ACCESSORS(c,	"dcblock", NULL, limisnap_setdcblock);
	CLASS_ATTR_STYLE(c, 	"dcblock", 0, "onoff");

	CLASS_ATTR_CHAR(c,		"mode",	0, t_limisnap, x_mode);
	CLASS_ATTR_LABEL(c,		"mode",	0, "Response Mode");
	CLASS_ATTR_ACCESSORS(c,	"mode",	NULL, limisnap_setmode);
	CLASS_ATTR_ENUMINDEX2(c, "mode",	0, "Linear", "Exponential");

	CLASS_ATTR_LONG(c,		"lookahead", 0, t_limisnap, x_lookahead);
	CLASS_ATTR_LABEL(c,		"lookahead", 0, "Lookahead");
	CLASS_ATTR_ACCESSORS(c,	"lookahead", NULL, limisnap_setlookahead);

	CLASS_ATTR_DOUBLE(c,	"preamp", 0, t_limisnap, x_preamp);
	CLASS_ATTR_LABEL(c,		"preamp", 0, "Preamp Gain");
	CLASS_ATTR_ACCESSORS(c,	"preamp", NULL, limisnap_setpreamp);
	
	CLASS_ATTR_DOUBLE(c,	"postamp", 0, t_limisnap, x_postamp);
	CLASS_ATTR_LABEL(c,		"postamp", 0, "Postamp Gain");
	CLASS_ATTR_ACCESSORS(c,	"postamp", NULL, limisnap_setpostamp);
	
	CLASS_ATTR_DOUBLE(c,	"threshold", 0, t_limisnap, x_threshold);
	CLASS_ATTR_LABEL(c,		"threshold", 0, "Threshold Level");
	CLASS_ATTR_ACCESSORS(c,	"threshold", NULL, limisnap_setthreshold);
	
	CLASS_ATTR_DOUBLE(c,	"release", 0, t_limisnap, x_release);
	CLASS_ATTR_LABEL(c,		"release", 0, "Release Time");
	CLASS_ATTR_ACCESSORS(c,	"release", NULL, limisnap_setrelease);
	
	class_dspinit(c);
	class_register(CLASS_BOX, c);
	s_limisnap_class = c;

	ps_gr = gensym("gr");
	ps_meters = gensym("meters");
}


void *limisnap_new(t_symbol *s, long argc, t_atom *argv)
{
	t_limisnap *x = (t_limisnap *)object_alloc(s_limisnap_class);
	long offset = attr_args_offset(argc, argv);

	//x->x_out = floatout(x);
	x->x_out = outlet_new((t_object *)x, NULL);

	x->x_buffersize = 512;
	x->x_channelcount = 1;
	if (offset) {
		x->x_channelcount = CLAMP(atom_getlong(argv), 1, MC_MAX_CHANS);
		if (offset > 1)
			x->x_buffersize = CLAMP(atom_getlong(argv+1), 100, 96000);
	}
	
#ifdef MC_VERSION
	dsp_setup((t_pxobject *)x, 1);
	outlet_new((t_object *)x, "multichannelsignal");
	x->x_obj.z_misc |= Z_NO_INPLACE | Z_MC_INLETS;
#else
    dsp_setup((t_pxobject *)x, x->x_channelcount);
    for (int i=0; i < x->x_channelcount; i++)
        outlet_new((t_object *)x, "signal");
    x->x_obj.z_misc |= Z_NO_INPLACE;
#endif
	
	x->x_limiter = new c74::min::lib::limiter(x->x_channelcount, x->x_buffersize, sys_getsr());

	x->x_bypass = 0;
	x->x_dcblock = 1;
	x->x_mode = 1;
	x->x_lookahead = 100;
	x->x_preamp = 0.0;
	x->x_postamp = 0.0;
	x->x_threshold = 0.0;
	x->x_release = 1000.0;

	x->p_clock = clock_new(x, (method)limisnap_tick);
	x->p_startclock = false;
	x->p_pollrate = 100;
	x->x_gr = 1.0;

	attr_args_process(x, argc, argv);

	return x;
}


void limisnap_free(t_limisnap*x)
{
	dsp_free((t_pxobject *)x);
	delete x->x_limiter;
	freeobject((t_object*)x->p_clock); // free the poll clock
}


void limisnap_assist(t_limisnap*x, void *b, long m, long a, char *s)
{
#ifdef MC_VERSION
	if (m == ASSIST_INLET)
		printf(s,"(multi-channel signal) Input");
	else
		sprintf(s,"(multi-channel signal) Output");
#else
    if (m == ASSIST_INLET)
        printf(s,"(signal) Input");
	else {
		if (a == x->x_channelcount) {
			sprintf(s, "(float) Gain Reduction");
		}
		else {
			sprintf(s, "(signal) Output");
		}
	}
#endif
}


long limisnap_multichanneloutputs(t_limisnap*x, int index)
{
	return x->x_channelcount;
}


long limisnap_inputchanged(t_limisnap*x, long index, long chans)
{
	if (chans != x->x_channelcount) {
		x->x_channelcount = chans;
		
		delete x->x_limiter;
		x->x_limiter = new c74::min::lib::limiter(x->x_channelcount, x->x_buffersize, sys_getsr());
	
		x->x_limiter->bypass(x->x_bypass);
		x->x_limiter->dcblock(x->x_dcblock);
		x->x_limiter->mode(static_cast<c74::min::lib::limiter::response_mode>(x->x_mode));
		x->x_limiter->lookahead(x->x_lookahead);
		x->x_limiter->preamp(x->x_preamp);
		x->x_limiter->postamp(x->x_postamp);
		x->x_limiter->threshold(x->x_threshold);
		x->x_limiter->release(x->x_release);
		return true;
	}
	return false;
}


void limisnap_clear(t_limisnap*x)
{
	x->x_limiter->clear();
}

void limisnap_tick(t_limisnap* x)
{
	// for the astute student of the Max SDK:
	//
	// this method is called by the scheduler thread
	// x->p_max is also accessed by the perform method in the audio thread
	// we could use a mutex or critical region to protect the following block of code from having the value of x->p_max modified during its execution.
	// however, mutexes and critical regions carry a performance penalty.
	//
	// in this case, due to the nature of what we are doing (drawing something every tenth of second showing the history of the previous samples),
	// the mutex or critical region will not add anything to the object, or protect us from crashes, and it carries a performance penalty.
	// so we have made a conscious decision to not use the aforementioned thread locking mechanisms.

	if (sys_getdspstate()) {
		// if the dsp is still on, schedule a next pictmeter_tick() call
		if (x->p_pollrate > 0) {
			x->x_gr = x->x_limiter->gr();
			outlet_float(x->x_out, x->x_gr);
			clock_fdelay(x->p_clock, x->p_pollrate);
		}
		else {
			clock_unset(x->p_clock);
		}
	}
}


void limisnap_bang(t_limisnap *x)
{
	// just output current values
	x->x_gr = x->x_limiter->gr();
	outlet_float(x->x_out, x->x_gr);

	//t_atom argv[1];
//atom_setfloat(argv + 0, 0.1);
//outlet_anything(x->x_out, ps_gr, 1, argv);
//outlet_anything(x->x_out, ps_meters, 1, argv);
}

t_max_err limisnap_setpollrate(t_limisnap* x, void* attr, long argc, t_atom* argv)
{
	// if poll is set to 0 then turn off the clock and just use bang to update
	x->p_pollrate = atom_getlong(argv);
	return MAX_ERR_NONE;
}

t_max_err limisnap_setbypass(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_bypass = atom_getlong(argv);
	x->x_limiter->bypass(x->x_bypass);
	return MAX_ERR_NONE;
}


t_max_err limisnap_setdcblock(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_dcblock = atom_getlong(argv);
	x->x_limiter->dcblock(x->x_dcblock);
	return MAX_ERR_NONE;
}


t_max_err limisnap_setmode(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_mode = atom_getlong(argv);
	x->x_limiter->mode( static_cast<c74::min::lib::limiter::response_mode>(x->x_mode) );
	return MAX_ERR_NONE;
}


t_max_err limisnap_setlookahead(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_lookahead = atom_getlong(argv);
	x->x_limiter->lookahead(x->x_lookahead);
	return MAX_ERR_NONE;
}


t_max_err limisnap_setpreamp(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_preamp = atom_getfloat(argv);
	x->x_limiter->preamp(x->x_preamp);
	return MAX_ERR_NONE;
}


t_max_err limisnap_setpostamp(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_postamp = atom_getfloat(argv);
	x->x_limiter->postamp(x->x_postamp);
	return MAX_ERR_NONE;
}


t_max_err limisnap_setthreshold(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_threshold = atom_getfloat(argv);
	x->x_limiter->threshold(x->x_threshold);
	return MAX_ERR_NONE;
}


t_max_err limisnap_setrelease(t_limisnap*x, void *attr, long argc, t_atom *argv)
{
	x->x_release = atom_getfloat(argv);
	x->x_limiter->release(x->x_release);
	return MAX_ERR_NONE;
}


void limisnap_perform64(t_limisnap*x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	//c74::min::audio
	c74::min::audio_bundle input(ins, numins, sampleframes);
	c74::min::audio_bundle output(outs, numouts, sampleframes);

	(*x->x_limiter)(input, output);

	if (x->p_startclock) {
		x->p_startclock = false;
		clock_fdelay(x->p_clock, 0);
	}
}


void limisnap_dsp64(t_limisnap*x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	x->x_limiter->reset(x->x_limiter->release(), x->x_limiter->mode(), samplerate);
	dsp_add64(dsp64, (t_object *)x, (t_perfroutine64)limisnap_perform64, 0, NULL);

	x->p_startclock = true;
	// only put perf func on dsp chain if sig is connected
	//if (count[0]) {
	//	object_method(dsp64, gensym("dsp_add64"), x, limisnap_perform64, 0, NULL);
	//	x->p_startclock = true;
	//}
}
