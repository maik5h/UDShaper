#pragma once

#include <chrono>
#include <iostream>
#include <fstream>
#include "../UDShaperElements/ShapeEditor.h"

// Get the current time since epoch in microseconds.
long getTime()
{
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// Class to measure the duration of ShapeEditor::forward calls with different numbers of ShapePoints.
//
// Measures the time for a single function call with input 1 when measure() is called. This is
// repeated numberSamples times, then a point is added to the ShapeEditor and it starts over until
// numberPoints is reached.
//
// Prints the data to a csv file with the following columns:
// - number of points in the editor
// - number of values contributing to the average
// - average time for the forward call in microseconds
// - sum of all output values (can be ignored, it is printed to the file to make sure the compiler
// does not optimize the function call away)
class ShapeEditorTimer
{
public:
  // The number of ShapePoints in the ShapeEditor.
  int numPoints = 1;

  // Create an AvgTimer.
  //
  // * @param outputPath Path to a .csv file to which the data is appended
  // * @param shapeEditor A ShapeEditor on which the measurement is performed
  // * @param numberSamples The number of samples averaged for each data point
  // * @param numberPoints The maximum number of ShapePoints to evaluate
  ShapeEditorTimer(std::string outputPath, ShapeEditor& shapeEditor, int numberSamples = 10000, int numberPoints = 100)
    : editor(shapeEditor)
  {
    outPath = outputPath;
    nMeasurementsMax = numberSamples;
    nPointsMax = numberPoints;
  }

  // Measure the time a SghapeEditor::forward call takes.
  //
  // Repeat this numberSamples * numberPoints times.
  void measure()
  {
    if (nPoints > nPointsMax) return;

    long startTime = getTime();
    // Make sure to save the output and print it to the file so that the compiler
    // does not skip this call.
    out += editor.forward(1.);
    sum += getTime() - startTime;
    nMeasurements += 1;

    // After the required number of measurements is done, save the data to the out file
    // and add a new point to ShapeEditor.
    if (nMeasurements == nMeasurementsMax)
    {
      double avg = static_cast<double>(sum) / nMeasurements;

      std::ofstream file(outPath, std::ios::app);
      file << nPoints << "," << nMeasurements << "," << avg << "," << out << "," << "\n";
      file.close();

      float pos = 1.f - static_cast<float>(nPoints) / (nPointsMax + 1);
      editor.insertPointAt(pos, pos);
      nPoints += 1;
      nMeasurements = 0;
      sum = 0;
    }
  }

private:
  // Path of a .csv file.
  std::string outPath;

  // Label of the data or type of measurement.
  ShapeEditor& editor;

  // number of measurements performed.
  int nMeasurements = 0;

  // Current number of points in editor.
  int nPoints = 1;

  // Number of samples to reach for a fixed number of points.
  int nMeasurementsMax;

  // Number of points to evauate at.
  int nPointsMax;

  // Sum of all durations between start() and stop() calls.
  long sum = 0;

  // Dummy variable to track the editor output.
  float out = 0;
};