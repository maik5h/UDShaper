#pragma once

#include <map>
#include <assert.h>
#include "IControls.h"
#include "../GUILayout.h"
#include "../color_palette.h"
#include "../assets.h"
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
  modulate     // Selects a parameter to be modulated by the active Envelope
};

// A point on a ShapeEditor that marks the transition between two curve segments.
// 
// Every ShapePoint lives on the area 0 <= x,y <= 1 and stores information about
// the curve segment to its left.
// A ShapeEditor has at least two ShapePoints:
//  1. one at (0, 0) which can not be moved or edited
//  2. one at (1, y) which can only be moved along the y-axis.
// An arbitrary amount of points may be added in between.
class ShapePoint;

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
  // Pointer to the ShapePoint that is currently being edited by the user.
  ShapePoint* currentlyDragging = nullptr;

  // Pointer to the ShapePoint that has been rightclicked by the user.
  ShapePoint* rightClicked = nullptr;

  // If a point was deleted, store a pointer to it here, so that the plugin can remove all
  // Envelope links after the input is fully processed.
  ShapePoint* deletedPoint = nullptr;

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

  // Pointer to the first ShapePoint in this ShapeEditor. ShapePoints are stored in a doubly linked list.
  //
  // The first point must be excluded from all methods since it can not be edited and displayed.
  // All loops over the ShapePoints must therefore start at shapePoints->next.
  ShapePoint* shapePoints = nullptr;

  // Create a ShapeEditor instance.
  // Each ShapeEditor carries a unique index, which identifies it from other instances.
  // * @param shapeEditorIndex Index of this instance. Must be unique between all ShapeEditor instances.
  ShapeEditor(int shapeEditorIndex);

  // Deletes all ShapePoints and frees the allocated memory.
  // TODO use vector instead of linked list and this is not necessary.
  ~ShapeEditor();

  // Assign an IRECT to this instance.
  // * @param rect The rectangle on the UI this instance will render in
  // * @param GUIWidth The width of the full UI in pixels
  // * @param GUIHeight The height of the full UI in pixels
  void setCoordinates(IRECT rect, float GUIWidth, float GUIHeight);

  // Finds the closest ShapePoint or curve center point to the coordinates (x, y).
  // Returns pointer to the corresponding point if it is closer than the squareroot
  // of minimumDistance, else nullptr. The field currentDraggingMode of this
  // ShapeEditor is set to either position or curveCenter, based on whether the point
  // or the curve center are closer.
  ShapePoint* getClosestPoint(float x, float y, float minimumDistance = REQUIRED_SQUARED_DISTANCE);

  // Deletes the rightClicked point.
  void deleteSelectedPoint();

  // Returns the pointer to the ShapePoint that was most recently deleted.
  // Returns the given pointer once and nullptr afterwards.
  ShapePoint* getDeletedPoint();

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
  const float forward(float input, double beatPosition = 0, double secondsPlayed = 0);

  // Attach the ShapeEditor UI to the given graphics context.
  //
  // This will create
  // - An orange background, white frame and grid
  // - A plot of the shaping function defined by this instance
  const void attachUI(IGraphics* g);

  // bool saveState(const clap_ostream_t *stream);
  // bool loadState(const clap_istream_t *stream, int version[3]);
};

class Envelope;

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

  void Draw(IGraphics& g) override;

  void OnResize() override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override;
  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override;

protected:
  ILayerPtr mLayer;
  float mMin = 0;
  float mMax = 1;
  bool mUseLayer = true;
  int mHorizontalDivisions = 4;
  int mVerticalDivisions = 4;
  IBlend gridBlend = IBlend(EBlend::Default, ALPHA_GRID);

  std::vector<float> mPoints;

  ShapeEditor* editor;
  IRECT editorRect;

  // The popup menu used to change the interpolation mode between points.
  IPopupMenu menu = IPopupMenu();
};