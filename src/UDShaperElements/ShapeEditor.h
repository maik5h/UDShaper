#pragma once

#include <assert.h>
#include <set>
#include "IControls.h"
#include "../GUILayout.h"
#include "../color_palette.h"
#include "../assets.h"
#include "../controlTags.h"
#include "../controlMessageTags.h"
using namespace iplug;
using namespace igraphics;

// Interpolation modes between two points of a ShapeEditor.
enum Shapes
{
  shapePower, // Curve follows shape of f(x) = (x < 0.5) ? x^power : 1-(1-x)^power, for 0 <= x <= 1, streched to the corresponding x and y intervals.
  shapeSine,  // Curve is a sine that continuously connects the previous and next point.
};

// Properties of a ShapePoint that can be edited inside a ShapeEditor.
enum editMode
{
  none,        // ignore
  position,    // Moves position of selected point.
  curveCenter, // Adjusts curveCenter and therefore parameter of curve function of next point to the right.
  modulate     // Selects a parameter to be modulated by the active LFO.
};

// A parameter that can be connected to an LFO link.
//
// Stores the indices of links it is connected to. To access the modulated value, an array
// of all modulation amplitudes must be forwarded to the get method.
//
// Note: The name might be misleading, this is not an iPlug2 or CLAP parameter, it is
// internal and the host does not see it.
class ModulatedParameter
{
private:
  // Base value of this ModulatedParameter.
  float base;

  // Minimum value this ModulatedParameter can take.
  float minValue;

  // Maximum value this ModulatedParameter can take.
  float maxValue;

  // Indices in the array of all modulation links that are connected to this parameter.
  std::set<int> modIndices;

public:
  // Create a ModulatedParameter.
  //
  // * @param base Initial base value, i.e. the value of this parameter if no modulation is active
  // * @param minValue The minimum value this parameter can take
  // * @param maxValue The maximum value this parameter can take
  ModulatedParameter(float base, float minValue = 0, float maxValue = 1);

  // Connects the LFO link at idx to this parameter.
  // * @return true if the link could be added and false if this parameter was already connected with the link.
  bool addModulator(int idx);

  // Adds the indices of all LFO links connected to this parameter to the given set.
  void getModulators(std::set<int>& mods) const;

  void removeModulator(int idx);

  // Sets the base value of this parameter to the input. Should be used when the parameter is
  // explicitly changed by the user.
  void set(float newValue);

  // Returns the current value, i.e. the modulation offsets added to the base clamped to the min and max values.
  // * @param modulationAmplitudes The array of all modulation amplitudes as set in LFOController::getModulationAmplitudes.
  // Can be nullptr, in which case the unmodulated base value is returned.
  float get(double* modulationAmplitudes) const;

  // Serializes the ModulatedParameter state.
  //
  // Saves:
  // - base value
  // - number of LFOs linked to this point
  // - the LFO link index of each link
  void serializeState(IByteChunk& chunk) const;

  // Unserializes the ModulatedParameter state from an IByteChunk object.
  // * @return The new chunk position
  int unserializeState(const IByteChunk& chunk, int startPos, int version);
};

// A point on a ShapeEditor that marks the transition between two curve segments.
// 
// Every ShapePoint lives on the area 0 <= x,y <= 1 and stores information about
// the curve segment to its left.
// A ShapeEditor has at least two ShapePoints:
//  1. one at (0, 0) which can not be moved or edited
//  2. one at (1, y) which can only be moved along the y-axis.
// An arbitrary amount of points may be added in between.
class ShapePoint
{
public:
  // Position and size of parent shapeEditor. Points must stay inside these coordinates.
  IRECT rect;

  // Relative x-position on the graph, 0 <= posX <= 1.
  ModulatedParameter posX;

  // Relative y-position on the graph, 0 <= posY <= 1.
  ModulatedParameter posY;

  // Omega after last update.
  float sineOmegaPrevious;

  // Omega for the shapeSine interpolation mode.
  float sineOmega;

  // Interpolation mode between this and the previous point.
  Shapes mode = shapePower;

  // Relative y-value of the curve at the x-center point between this and the previous point.
  ModulatedParameter curveCenterPosY;

  // The parent ShapeEditor will highlight this point or curve center point if highlightMode is not modNone.
  // modPosX and modPosY highlight the point itself, modCurveCenterY highlight the curve center.
  modulationMode highlightMode = modNone;

  // Construct a ShapePoint.
  //
  // * @param x Normalized x-position of the point on the parent editor graph
  // * @param y Normalized y-position of the point on the parent editor graph
  // * @param editorRect Area of the parent editor on the UI
  // * @param pow Initial power value
  // * @param omega Inital omega value
  // * @param initMode Initial curve segment interpolation mode
  ShapePoint(float x, float y, IRECT editorRect, float pow = 1, float omega = 0.5, Shapes initMode = shapePower);

  // Sets relative positions posX and posY such that the absolute positions are equal to the input x and y.
  void updatePositionAbsolute(float x, float y);

  // get-functions for parameters that are dependent on a ModulatedParameter and have to recalculated each time
  // they are used:

  // Returns the relative x-position of this point.
  // * @param modulationAmplitudes Pointer to an array of modulation amplitudes. Can be nullptr, in which case
  // the unmmodulated value is returned.
  // * @param lowerBound If the parameter is modulated, it can not move below this x-value. Effectively makes
  // modulated points push their neighbors to the right.
  float getPosX(double* modulationAmplitudes = nullptr, float lowerBound = 0) const;

  // Returns the relative y-position of the point.
  float getPosY(double* modulationAmplitudes = nullptr) const;

  // Calculates the absolute x-position of the point on the GUI from the relative position posX.
  float getAbsPosX(double* modulationAmplitudes = nullptr, float lowerBound = 0) const;

  // Calculates the absolute y-position of the point on the GUI from the relative position posY.
  float getAbsPosY(double* modulationAmplitudes = nullptr) const;

  // Calculates the absolute y-position of the curve center associated with this point.
  //
  // * @param previousY The absolute y-position of the previous point
  // * @param modulationAmplitudes Pointer to an array of modulation amplitudes. Can be nullptr, in which case
  // the unmmodulated value is returned.
  float getCurveCenterAbsPosY(float previousY, double* modulationAmplitudes = nullptr) const;

  // Update the Curve center point when manually dragging it.
  //
  // * @param x Target x-position
  // * @param y Target y-position
  // * @param previousY The absolute y-position of the previous point
  void updateCurveCenter(float x, float y, float previousY);

  void processLeftClick();

  // Serializes the ShapePoint state.
  void serializeState(IByteChunk& chunk) const;

  // Unserialize the ShapePoint state.
  // * @return The new chunk position
  int unserializeState(const IByteChunk& chunk, int startPos, int version);
};

// A graph editor that can be used to design functions on the user interface.
// The function defined is a mapping from [0, 1] to [0, 1] which is accessible through
// the forward() method.
//
// ShapeEditors are built around ShapePoints, which are points that exist on the interface of the editor.
// The graph is interpolated between neighbouring points. The default interpolation is linear.
// Points can be added, removed and moved freely between the neighbouring points.
class ShapeEditor
{
  protected:
  // Index of the ShapePoint that is currently being edited by the user.
  int currentlyDraggingIdx = -1;

  // Index of the ShapePoint that has been rightclicked by the user.
  int rightClickedIdx = -1;

  public:
  // Stores the box coordinates of GUI elements of this ShapeEditor instance.
  ShapeEditorLayout layout;

  // Can be used to distinguish this ShapeEditor instance from others.
  const int index;

  // Indicates which property of a ShapePoint should be adressed when processing e.g.
  // mouse drag or right click.
  editMode currentEditMode = none;

  // Set to true to draw circles around ShapePoints and curve center points.
  bool highlightModulatedParameters = false;

  // Vector of the ShapePoints defining the shaping function.
  //
  // The first point must be excluded from all methods since it can not be edited and displayed.
  // All loops over the ShapePoints must therefore start at shapePoints[1].
  std::vector<ShapePoint> shapePoints = {};

  // Create a ShapeEditor instance.
  // Each ShapeEditor carries a unique index, which identifies it from other instances.
  // * @param rect The rectangle on the UI this instance will render in
  // * @param GUIWidth The width of the full UI in pixels
  // * @param GUIHeight The height of the full UI in pixels
  // * @param shapeEditorIndex Index of this instance. Must be unique between all ShapeEditor instances.
  ShapeEditor(IRECT rect, float GUIWidth, float GUIHeight, int shapeEditorIndex);

  // Finds the closest ShapePoint or curve center point to the coordinates (x, y).
  // Returns the index of the corresponding point if it is closer than the squareroot
  // of minimumDistance, else -1. The field currentEditMode of this
  // ShapeEditor is set to either position or curveCenter, based on whether the point
  // or the curve center are closer.
  int getClosestPoint(float x, float y, float minimumDistance = REQUIRED_SQUARED_DISTANCE);

  // Deletes the rightClicked point.
  void deleteSelectedPoint();

  // Initializes the dragging of a point if clicked close to a ShapePoint.
  void processLeftClick(float x, float y);

  // Can either move a ShapePoint or change the interpolation shape between two points.
  void processMouseDrag(float x, float y);

  // Can either:
  //  - Delete a ShapePoint if (x, y) is close to point.
  //  - Reset the interpolation shape if (x, y) is close to a curve center point.
  void processDoubleClick(float x, float y);

  // Can either:
  //  - Reset the interpolation shape if (x, y) is close to a curve center point (returns false).
  //  - Return true if the context menu to select the interpolation mode needs to be opened after
  //    clicking close to a point.
  //  - Add a new ShapePoint at (x, y) if not close to a point or curve center.
  bool processRightClick(float x, float y);

  // Set the interpolation mode of the rightclicked point to shape.
  // Resets the rightClicked attribute to nullptr.
  void setInterpolationMode(Shapes shape);

  // Passes input to the function defined by the graph and returns the function value at this position.
  // Input is clamped to [0, 1].
  // * @param input Input value
  // * @param modulationAmplitudes Array of the amplitudes of all LFO modulation links. Can be nullptr,
  // in which case the unmodulated base values are used to calculate the output. If not nullptr, this must
  // point to an array of size MAX_NUMBER_LFOS * MAX_MODULATION_LINKS.
  const float forward(float input, double* modulationAmplitudes = nullptr);

  // Attach the ShapeEditor UI to the given graphics context.
  //
  // This will create
  // - An orange background, white frame and grid
  // - A plot of the shaping function defined by this instance
  const void attachUI(IGraphics* g);

  // Disconnect the link with idx from all modulated parameters.
  void disconnectLink(int linkIdx);

  // Adds the indices of all links corresponding to parameters of
  // this editor to the given set.
  void getLinks(std::set<int>& links);

  // Used for adding points without user input when testing performance.
  void insertPointAt(float x, float y);

  bool serializeState(IByteChunk& chunk) const;
  int unserializeState(const IByteChunk& chunk, int startPos, int version);
};

// Carries information necessary to connect an LFO to a ModulatedParameter.
//
// x: The x-position of the cursor from which the message was sent
// y: The y-position of the cursor from which the message was sent
// idx: The index of the LFO link to which the parameter should connect
struct LFOConnectInfo
{
  float x;
  float y;
  int idx;
};

// UI representation of a ShapeEditor.
//
// Inherits from IControl and can be attached to a graphics context.
// It handles the rendering and forwards user inputs to a ShapeEditor instance.
class ShapeEditorControl : public IControl, public IVectorBase
{
public:
  // Constructs a ShapeEditorControl.
  // * @param bounds The control's total bounds
  // * @param editorBounds The bounds of the graph editor
  // * @param shapeEditor Pointer to the ShapeEditor corresponding to this control
  // * @param numPoints The number of points used to draw the functions
  // * @param useLayer A flag to draw the control layer
  ShapeEditorControl(const IRECT& bounds, const IRECT& editorBounds, ShapeEditor* shapeEditor, int numPoints, bool useLayer = false);

  // Sets the ShapeEditor associated with this control to editor.
  void setEditor(ShapeEditor* editor);

  void Draw(IGraphics& g) override;

  void OnResize() override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override;
  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override;
  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override;

  // Array of modulationAmplitudes stored on plugin level that is updated frequently
  // on the UI thread and represents the current modulation state.
  double* modulationAmplitudes = nullptr;

protected:
  ILayerPtr mLayer;
  float mMin = 0;
  float mMax = 1;
  bool mUseLayer = true;
  int mHorizontalDivisions = 4;
  int mVerticalDivisions = 4;
  IBlend gridBlend = IBlend(EBlend::Default, ALPHA_GRID);

  std::vector<float> mPoints;

  ShapeEditor* editor = nullptr;
  IRECT editorRect;

  // The popup menu used to change the interpolation mode between points.
  IPopupMenu menu = IPopupMenu();

  // Popup menu used to choose between x- and y-modulation for ShapePoints.
  IPopupMenu menuMod = IPopupMenu();

  // Stores the index of the ShapePoint corresponding to menuMod while the menu is open.
  int modPointIdx = -1;

  // Stores the modulation index corresponding to menuMod while the menu is open.
  int modIdx = -1;
};