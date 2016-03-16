#include <stdio.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <ApplicationServices/ApplicationServices.h>

static float currBrightness = HUGE_VALF;

void setBrightness(float *brightness) {
  io_service_t      service;
  CGDirectDisplayID targetDisplay;

  CFStringRef key = CFSTR(kIODisplayBrightnessKey);

  targetDisplay = CGMainDisplayID();
  service = CGDisplayIOServicePort(targetDisplay);

  if (*brightness != HUGE_VALF) { // set the brightness, if requested
    IODisplaySetFloatParameter(service, kNilOptions, key, *brightness);
  } else {
    IODisplayGetFloatParameter(service, kNilOptions, key, brightness);
  }
}

void IncreaseBrightness() {
  if (currBrightness == HUGE_VALF) {
    setBrightness(&currBrightness);
  }
  currBrightness += 0.1;
  setBrightness(&currBrightness);
}

void DecreaseBrightness() {
  if (currBrightness == HUGE_VALF) {
    setBrightness(&currBrightness);
  }
  currBrightness -= 0.1;
  setBrightness(&currBrightness);
}
