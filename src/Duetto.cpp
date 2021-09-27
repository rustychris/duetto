#include "plugin.hpp"

#define NNOTES 10
#define NSTEPS 8

struct Duetto : Module {
  enum ParamIds {
    NOTE00_PARAM,
    NOTE01_PARAM,
    NOTE02_PARAM,
    NOTE03_PARAM,
    NOTE04_PARAM,
    NOTE05_PARAM,
    NOTE06_PARAM,
    NOTE07_PARAM,
    NOTE08_PARAM,
    NOTE09_PARAM,
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
    ENUMS(STEP_LIGHT, 8),
    // LIGHT03_LIGHT,
    // LIGHT04_LIGHT,
    // LIGHT02_LIGHT,
    // LIGHT05_LIGHT,
    // LIGHT01_LIGHT,
    // LIGHT06_LIGHT,
    // LIGHT00_LIGHT,
    // LIGHT07_LIGHT,
    NUM_LIGHTS
  };

  float noteValues[NNOTES]={-12/12.,-9/12.,-7/12., -5/12.,-2./12,
                             0, 3/12., 5/12., 7/12., 10/12.};

  dsp::BooleanTrigger noteTriggers[NNOTES];
  
  float phase; // signal output
  float metroPhase; // metronome/clock
  float pitches[NSTEPS];
  int step;
  ParamIds NoteParams[NNOTES]={NOTE00_PARAM,
                               NOTE01_PARAM,
                               NOTE02_PARAM,
                               NOTE03_PARAM,
                               NOTE04_PARAM,
                               NOTE05_PARAM,
                               NOTE06_PARAM,
                               NOTE07_PARAM,
                               NOTE08_PARAM,
                               NOTE09_PARAM};

  Duetto() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(NOTE00_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE01_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE02_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE03_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE04_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE05_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE06_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE07_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE08_PARAM, 0.f, 1.f, 0.f, "");
    configParam(NOTE09_PARAM, 0.f, 1.f, 0.f, "");

    for(int i=0;i<NSTEPS;i++) {
      pitches[i]=-10; // off.
    }
    step=0;
    metroPhase=0.f;

  }

  void process(const ProcessArgs &args) override {
    // Clock
    float tempo=1.f; // 120 bpm
    float clockTime = std::pow(2.f, tempo); // tempo in Hz

    metroPhase += clockTime * args.sampleTime;
    if (metroPhase >= 1.f) {
      // Turn current light off:
      lights[STEP_LIGHT+step].value = 0;
      step++;
      metroPhase-=1.f;
      if (step>=NSTEPS)
        step=0;
      lights[STEP_LIGHT+step].value = 1.f;
    }
    
    for (int t=0;t<NNOTES;t++ ) {
      if (noteTriggers[t].process(params[NoteParams[t]].getValue() > 0.f)) {
        pitches[step]=noteValues[t];
      }
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

    addParam(createParamCentered<CKD6>(mm2px(Vec(13.215,  99.829)), module, Duetto::NOTE00_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(26.830,  99.829)), module, Duetto::NOTE01_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(40.444,  99.829)), module, Duetto::NOTE02_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(54.059,  99.829)), module, Duetto::NOTE03_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(67.674,  99.829)), module, Duetto::NOTE04_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(13.215, 112.498)), module, Duetto::NOTE05_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(26.830, 112.498)), module, Duetto::NOTE06_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(40.444, 112.498)), module, Duetto::NOTE07_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(54.059, 112.498)), module, Duetto::NOTE08_PARAM));
    addParam(createParamCentered<CKD6>(mm2px(Vec(67.674, 112.498)), module, Duetto::NOTE09_PARAM));

    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(88.257, 58.196)), module, Duetto::OUT1_OUTPUT));

    // Color changing? First just show the tempo
    float centerX=37,centerY=68, rad=15;
    for ( int step=0;step<NSTEPS;step++) {
      float theta=step*2*M_PI/NSTEPS;
      float x=centerX + rad*std::cos(theta);
      float y=centerY + rad*std::sin(theta);
      addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(x,y)), module, Duetto::STEP_LIGHT+step));
    }
  }
};


Model* modelDuetto = createModel<Duetto, DuettoWidget>("Duetto");


