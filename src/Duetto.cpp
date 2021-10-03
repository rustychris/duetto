#include "plugin.hpp"

#define NNOTES 10
#define NSTEPS 8

#define NCOLORS 3

const float MIN_TIME = 1e-3f;
const float MAX_TIME = 10.f;
const float LAMBDA_BASE = MAX_TIME / MIN_TIME;

struct SlidePot : app::SvgSlider {
  SlidePot() {
    math::Vec margin = math::Vec(3.5, 3.5);
    maxHandlePos = math::Vec(-1, -2).plus(margin);
    minHandlePos = math::Vec(-1, 87).plus(margin);
    setBackgroundSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/BefacoSlidePot.svg")));
    setHandleSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/BefacoSlidePotHandle.svg")));
    background->box.pos = margin;
    box.size = background->box.size.plus(margin.mult(2));
  }
};



struct Duetto : Module {
  enum ParamIds {
    ENUMS(NOTE_PARAM,NNOTES),
    ENUMS(STEP_PARAM, NSTEPS),
    TEMPO_PARAM,
    NUM_PARAMS
  };
  enum InputIds {
    NUM_INPUTS
  };
  enum OutputIds {
    OUT1_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds {
    ENUMS(STEP_LIGHT, NCOLORS*NSTEPS),
    NUM_LIGHTS
  };

  dsp::MinBlepGenerator<16, 16, float> sawMinBlep;

  // Minor pentatonic. and an Off note
  float noteValues[NNOTES+1]={-12/12.,-9/12.,-7/12., -5/12.,-2./12,
                            0, 3/12., 5/12., 7/12., 10/12.,
                            -10};

  // why is this necessary?
  dsp::BooleanTrigger noteTriggers[NNOTES];
  dsp::BooleanTrigger stepTriggers[NSTEPS];
  
  float phase; // signal output
  float metroPhase; // metronome/clock
  float pitches[NSTEPS];
  int step;

  float pitch;
  float env; // envelope
  float attack=0.1,decay=0.50;
  float attacking;
  
  float stepColor[NSTEPS][NCOLORS];
  float noteColor[NNOTES+1][NCOLORS]={ {1.0, 0.0, 0.0},
                                       {0.5, 0.5, 0.0},
                                       {0.0, 1.0, 0.0},
                                       {0.0, 0.5, 0.5},
                                       {0.0, 0.0, 1.0},
                                       
                                       {1.0, 0.0, 0.0},
                                       {0.5, 0.5, 0.0},
                                       {0.0, 1.0, 0.0},
                                       {0.0, 0.5, 0.5},
                                       {0.0, 0.0, 1.0},

                                       {1.0, 1.0, 1.0}, // no note
  };
  
  Duetto() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    for(int n=0;n<NNOTES;n++)
      configParam(NOTE_PARAM+n, 0.f, 1.f, 0.f, "Note");
    for(int s=0;s<NSTEPS;s++)
      configParam(STEP_PARAM+s, 0.f, 1.f, 0.f, "Step");

    // Tempo in BPM is 100*voltage.
    // for labeling, think of BPM as quarter notes, and the
    // sequence plays eighths. 
    configParam(TEMPO_PARAM, 0.f, 6.0f, 2.40f, "Tempo","bpm",
                0.f,50.0);
    
    for(int i=0;i<NSTEPS;i++) {
      setStepNote(i,NNOTES,0.5); // no note, inactive
    }

    step=0;
    metroPhase=0.f;
    phase=0.f;
    env=0.f;
    attacking=0.f;
    // pitch itself should always be valid,
    // and let env track whether there is a note
    pitch=0.f;
  }

  void setStepNote(int step,int note,float bright) {
    // step: 0 to NSTEPS-1
    // note: -1 to leave as is, 0 to NNOTES-1 to set note, NNOTES for no note
    if ( note>=0 ) {
      pitches[step]=noteValues[note];
      stepColor[step][0]=noteColor[note][0];
      stepColor[step][1]=noteColor[note][1];
      stepColor[step][2]=noteColor[note][2];
    } else {
      ; // leave note as is
    }

    if (bright>=0.0) {
      lights[STEP_LIGHT+step*NCOLORS+0].value = bright*stepColor[step][0];
      lights[STEP_LIGHT+step*NCOLORS+1].value = bright*stepColor[step][1];
      lights[STEP_LIGHT+step*NCOLORS+2].value = bright*stepColor[step][2];
    } else {
      ; // no GUI update
    }
  }

  void process(const ProcessArgs &args) override {
    // Clock
    float tempo=params[TEMPO_PARAM].getValue();
    // float clockTime = std::pow(2.f, tempo); // tempo in Hz
    float clockTime = 100.f/60.f * tempo;
    
    for (int t=0;t<NNOTES;t++ ) {
      if (noteTriggers[t].process(params[NOTE_PARAM+t].getValue() > 0.f)) {
        setStepNote(step,t,1.0);
        // force an attack
        attacking=1;
        pitch=pitches[t];
      }
    }

    for (int s=0;s<NSTEPS;s++ ) {
      if (stepTriggers[s].process(params[STEP_PARAM+s].getValue() > 0.f)) {
        setStepNote(s,NNOTES,0.5); // default to inactive
      }
    }
    
    metroPhase += clockTime * args.sampleTime;
    if (metroPhase >= 1.f) {
      // Turn current light off:
      setStepNote(step,-1,0.5);
      
      step++;
      metroPhase-=1.f;
      if (step>=NSTEPS)
        step=0;

      setStepNote(step,-1,1.0);

      if(pitches[step]>-4.f) {
        attacking=1; // start envelope attack
        pitch=pitches[step];
      }
    }
        
    // Compute the frequency from the pitch parameter and input
    // = pitches[step];

    float out;
    
    // The default pitch is C4 = 261.6256f
    float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
      
    // Accumulate the phase
    phase += freq * args.sampleTime;
    if (phase >= 1.0f)
      phase -= 1.f;
      
    if ( false ) { // Sine
      out = std::sin(2.f * M_PI * (phase-0.5));
    } else if (true) { // Saw
      // Based on Fundamentals VCO
      // deltaPhase is just how much the phase is changing
      // in this step.
      float deltaPhase = math::clamp(freq * args.sampleTime, 1e-6f, 0.35f);

      // calculate when during this iteration the phase crossed 0.5 as
      // a value in [0,1]. If halfCrossing is outside that range, then
      // the crossing didn't occur during this step.
      
      float halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;
      if ( (0 < halfCrossing) & (halfCrossing <= 1.f) ) {
        float p = halfCrossing - 1.f; // how long before this sample did we cross?
        sawMinBlep.insertDiscontinuity(p, -2.f);
      }

      float x = phase + 0.5f; // phase is 0 to 1, so this is 0.5 to 1.5
      x -= int(x); // for the first half, this is just 0.5.. 1.0
      // then at phase=0.5, it drops to 0, and climbs to 0.5
      out = 2 * x - 1; // scale to [-1,1]
      out += sawMinBlep.process();
    }

    // envelope:
    // attack = simd::clamp(attack, 0.f, 1.f);
    // decay = simd::clamp(decay, 0.f, 1.f);
    float attackLambda = std::pow(LAMBDA_BASE, -attack) / MIN_TIME;
    float decayLambda = std::pow(LAMBDA_BASE, -decay) / MIN_TIME;

    // Get target and lambda for exponential decay
    float target=0.f;
    float lambda=decayLambda;
    
    if( attacking ) {
      target=1.2f;
      lambda=attackLambda;
    }
    
    // Adjust env -- so lambda is the e-folding time frequency?
    env += (target - env) * lambda * args.sampleTime;

    // Turn off attacking state if envelope is HIGH
    if ( env>=1.f ) {
      attacking=0.f;
    }

    // ADSR had this, not sure why
    // Turn on attacking state if gate is LOW
    // attacking[c / 4] = simd::ifelse(gate[c / 4], attacking[c / 4], float_4::mask());
    
    // Audio signals are typically +/-5V
    // https://vcvrack.com/manual/VoltageStandards
    outputs[OUT1_OUTPUT].setVoltage(5.f * env * out);
  }
};


struct DuettoWidget : ModuleWidget {
  DuettoWidget(Duetto* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Duetto.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addParam(createParamCentered<CKD6>(mm2px(Vec(13.215,  99.829)), module, Duetto::NOTE_PARAM+0));
    addParam(createParamCentered<CKD6>(mm2px(Vec(26.830,  99.829)), module, Duetto::NOTE_PARAM+1));
    addParam(createParamCentered<CKD6>(mm2px(Vec(40.444,  99.829)), module, Duetto::NOTE_PARAM+2));
    addParam(createParamCentered<CKD6>(mm2px(Vec(54.059,  99.829)), module, Duetto::NOTE_PARAM+3));
    addParam(createParamCentered<CKD6>(mm2px(Vec(67.674,  99.829)), module, Duetto::NOTE_PARAM+4));
    addParam(createParamCentered<CKD6>(mm2px(Vec(13.215, 112.498)), module, Duetto::NOTE_PARAM+5));
    addParam(createParamCentered<CKD6>(mm2px(Vec(26.830, 112.498)), module, Duetto::NOTE_PARAM+6));
    addParam(createParamCentered<CKD6>(mm2px(Vec(40.444, 112.498)), module, Duetto::NOTE_PARAM+7));
    addParam(createParamCentered<CKD6>(mm2px(Vec(54.059, 112.498)), module, Duetto::NOTE_PARAM+8));
    addParam(createParamCentered<CKD6>(mm2px(Vec(67.674, 112.498)), module, Duetto::NOTE_PARAM+9));

    //addParam(createParam<LEDSlider>(mm2px(Vec(10.0, 60)), module, Duetto::TEMPO_PARAM));
    addParam(createParam<SlidePot>(mm2px(Vec(10.0, 50)), module, Duetto::TEMPO_PARAM));

    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.257, 58.196)), module, Duetto::OUT1_OUTPUT));

    // Color changing? First just show the tempo
    float centerX=37,centerY=68, rad=15;
    for ( int step=0;step<NSTEPS;step++) {
      float theta=step*2*M_PI/NSTEPS;
      float x=centerX + rad*std::cos(theta);
      float y=centerY + rad*std::sin(theta);
      
      // Following Mutes from Fundamental
      addParam( createParamCentered<LEDBezel>(mm2px(Vec(x,y)), module, Duetto::STEP_PARAM + step));
      
      addChild( createLightCentered<LEDBezelLight<RedGreenBlueLight>>(mm2px(Vec(x,y)), module,
                                                                      Duetto::STEP_LIGHT + step*NCOLORS));
    }
  }
};


Model* modelDuetto = createModel<Duetto, DuettoWidget>("Duetto");


// NEXT:
//  Filter -- 
//  Delay
//  Dual Oscillator
