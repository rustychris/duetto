#include "plugin.hpp"

#define NNOTES 10
#define NSTEPS 8

#define NCOLORS 3

struct Duetto : Module {
  enum ParamIds {
    ENUMS(NOTE_PARAM,NNOTES),
    ENUMS(STEP_PARAM, NSTEPS),
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
      configParam(NOTE_PARAM+n, 0.f, 1.f, 0.f, "");

    for(int i=0;i<NSTEPS;i++) {
      setStepNote(i,NNOTES,0.5); // no note, inactive
    }

    step=0;
    metroPhase=0.f;
    phase=0.f;
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
    float tempo=1.f; // 120 bpm
    float clockTime = std::pow(2.f, tempo); // tempo in Hz

    for (int t=0;t<NNOTES;t++ ) {
      if (noteTriggers[t].process(params[NOTE_PARAM+t].getValue() > 0.f)) {
        setStepNote(step,t,1.0); 
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
    }
        
    // Compute the frequency from the pitch parameter and input
    float pitch = pitches[step];

    float out;
    
    if (pitch<-4.f) {
      out=0.f;
    } else {
      // pitch = clamp(pitch, -4.f, 4.f);
    
      // The default pitch is C4 = 261.6256f
      float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
      
      // Accumulate the phase
      phase += freq * args.sampleTime;
      if (phase >= 0.5f)
        phase -= 1.f;
      
      // Compute the sine output
      out = std::sin(2.f * M_PI * phase);
    }
    
    // Audio signals are typically +/-5V
    // https://vcvrack.com/manual/VoltageStandards
    outputs[OUT1_OUTPUT].setVoltage(5.f * out);
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
//  Update lights at init, and when their state changes.


