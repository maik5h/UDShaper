#include "ShapeEditor.h"


// Calculate the power such that the function f: [0, 1] -> [0, 1], f(x) = x^power
// satisfies the following equations:
//  f(0.5) = posY           if posY <= 0.5
//  f(0.5) = 1 - posY       if posY > 0.5
//
// In the second case, -power is returned instead of power. It does NOT mean the curve
// is actually f(x) = 1 / (x^|power|) and only indicates the input was > 0.5.
static float getPowerFromPosY(float posY)
{
  float power = (posY < 0.5) ? log(posY) / log(0.5) : log(1 - posY) / log(0.5);

  // Clamp to allowed maximum power.
  power = (power > SHAPE_MAX_POWER) ? SHAPE_MAX_POWER : power;

  return (posY > 0.5) ? -power : power;
}

class ModulatedParameter
{
private:
  // Base value of this ModulatedParameter.
  float base;

  // Minimum value this ModulatedParameter can take.
  float minValue;

  // Maximum value this ModulatedParameter can take.
  float maxValue;

  // Pointer to the ShapePoint that this parameter is part of.
  const ShapePoint* parent;

  // The modulation amount of every Envelope associated with this ModulatedParameter.
  std::map<Envelope*, float> modulationAmounts;

public:
  // The type of parameter represented by this instance.
  const modulationMode mode;

  // Create a ModulatedParameter.
  // 
  // ModulatedParameters are internal parameters that are not reported to the host.
  // They can only exist as members of ShapePoints.
  // * @param parentPoint Pointer to the ShapePoint which this parameter belongs to
  // * @param inMode The type of modulation this parameter describes. Can be the x-position, y-position or curve center position of a ShapePoint
  // * @param inBase Initial base value, i.e. the value this parameter takes if no modulation is active
  // * @param inMinValue The minimum value this parameter can take
  // * @param inMaxValue The maximum value this parameter can take
  ModulatedParameter(ShapePoint* parentPoint, modulationMode inMode, float inBase, float inMinValue = -1, float inMaxValue = 1)
    : parent(parentPoint)
    , mode(inMode)
  {
    base = inBase;
    minValue = inMinValue;
    maxValue = inMaxValue;
  }

  // Registers the given envelope as a modulator to this parameter. This Envelope will now contribute
  // to the modulation when calling .get() with the given amount.
  // Returns true on success and false if the Envelope was already a Modulator of this ModulatedParameter.
  bool addModulator(Envelope* envelope, float amount)
  {
    // Envelopes can only be added once.
    for (auto a : modulationAmounts)
    {
      if (a.first == envelope)
      {
        return false;
      }
    }

    modulationAmounts.at(envelope) = amount;
    return true;
  }

  void removeModulator(Envelope* envelope) { modulationAmounts.erase(envelope); }

  // Returns the modulation amount of the Envelope at the given pointer.
  const float getAmount(Envelope* envelope) { return modulationAmounts.at(envelope); }

  // Sets the modulation amount of the Envelope at the given pointer to the given amount.
  void setAmount(Envelope* envelope, float amount)
  {
    amount = (amount < -1) ? -1 : (amount > 1) ? 1 : amount;
    modulationAmounts.at(envelope) = amount;
  }

  // Returns a pointer to the ShapePoint that uses this ModulatedParameter instance.
  const ShapePoint* getParentPoint() { return parent; }

  // Returns a pointer to the base value of this ModulatedParameter.
  // This is used solely for serialization in ShapeEditor::saveState.
  const float* getBaseAddress() { return &base; }

  // Sets the base value of this parameter to the input. Should be used when the parameter is
  // explicitly changed by the user.
  void set(float newValue)
  {
    base = (newValue < minValue) ? minValue : newValue;
    base = (newValue > maxValue) ? maxValue : newValue;
  }

  // Returns the current value, i.e. the modulation offsets added to the base clamped to the min and max values.
  const float get(double beatPosition = 0, double secondsPlayed = 0.) {
    float currentValue = base;
    // TODO unquote once Envelopes are implemented.
    // Iterate over modulationAmounts, first is pointer to Envelope, second is amount.
    //for (auto a : modulationAmounts)
    //{
    //  currentValue += a.second * a.first->modForward(beatPosition, secondsPlayed);
    //}

    currentValue = (currentValue > maxValue) ? maxValue : (currentValue < minValue) ? minValue : currentValue;
    return currentValue;
  }
};


class ShapePoint
{
public:
  // parameters of the parent ShapeEditor:
  
  // Position and size of parent shapeEditor. Points must stay inside these coordinates.
  IRECT rect;

  // Index of the parent ShapeEditor.
  const int shapeEditorIndex;

  // Parameters of the point:
  
  // Relative x-position on the graph, 0 <= posX <= 1.
  ModulatedParameter posX;

  // Relative y-position on the graph, 0 <= posY <= 1.
  ModulatedParameter posY;

  // Pointer to the previous ShapePont.
  ShapePoint* previous = nullptr;

  // Pointer to the next ShapePoint.
  ShapePoint* next = nullptr;

  // Parameters of the curve segment:
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
  // * @param parentIdx Index of the parent editor
  // * @param pow Initial power value
  // * @param omega Inital omega value
  // * @param initMode Initial curve segment interpolation mode
  ShapePoint(float x, float y, IRECT editorRect, const int parentIdx, float pow = 1, float omega = 0.5, Shapes initMode = shapePower)
    : shapeEditorIndex(parentIdx)
    , posX(this, modPosX, x, 0)
    , posY(this, modPosY, y, 0)
    , curveCenterPosY(this, modCurveCenterY, std::pow(0.5, pow), 0)
  {
    assert((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));

    rect = editorRect;
    mode = initMode;
    sineOmega = omega;
    sineOmegaPrevious = omega;
  }

  // Sets relative positions posX and posY such that the absolute positions are equal to the input x and y.
  // Updates the curve center.
  void updatePositionAbsolute(float x, float y)
  {
    // Move the modulatable relative parameters to the corresponding positions.
    posX.set((x - rect.L) / rect.W());
    posY.set((rect.B - y) / rect.H());
  }

  // get-functions for parameters that are dependent on a ModulatedParameter and have to recalculated each time
  // they are used:

  // Returns the relative x-position of this point.
  const float getPosX(double beatPosition = 0., double secondsPlayed = 0.)
  {
    // Rightmost point must always be at x = 1;
    if (next == nullptr)
    {
      return 1;
    }

    // X-position is limited by the position of the next point. Modulating
    // a point to the left will push all other points on its way to the left.
    // Mod to the right will stop on the next point.
    else
    {
      float x = posX.get(beatPosition, secondsPlayed);
      float nextX = next->getPosX(beatPosition, secondsPlayed);
      return (x > nextX) ? nextX : x;
    }
  }

  // Returns the relative y-position of the point.
  const float getPosY(double beatPosition = 0., double secondsPlayed = 0.)
  {
    return posY.get(beatPosition, secondsPlayed);
  }

  // Calculates the absolute x-position of the point on the GUI from the relative position posX.
  const float getAbsPosX(double beatPosition = 0., double secondsPlayed = 0.)
  {
    return rect.L + getPosX(beatPosition, secondsPlayed) * rect.W();
  }

  // Calculates the absolute y-position of the point on the GUI from the relative position posY.
  const float getAbsPosY(double beatPosition = 0., double secondsPlayed = 0.)
  {
    // By convention, absolute position is inversely related to posY.
    return rect.B - getPosY(beatPosition, secondsPlayed) * rect.H();
  }

  // Calculates the absolute x-position of the curve center associated with this point.
  const float getCurveCenterAbsPosX(double beatPosition = 0., double secondsPlayed = 0.)
  {
    return (getAbsPosX(beatPosition, secondsPlayed) + previous->getAbsPosX(beatPosition, secondsPlayed)) / 2;
  }

  // Calculates the absolute y-position of the curve center associated with this point.
  const float getCurveCenterAbsPosY(double beatPosition = 0., double secondsPlayed = 0.)
  {
    float y = getAbsPosY(beatPosition, secondsPlayed);
    float yL = previous->getAbsPosY(beatPosition, secondsPlayed);
    float curveCenterY = curveCenterPosY.get(beatPosition, secondsPlayed);

    float yExtent = y - yL;

    return yL + curveCenterY * yExtent;
  }

  // Update the Curve center point when manually dragging it.
  void updateCurveCenter(float x, float y)
  {
    // Nothing to update if curve segment is flat.
    if (previous->getAbsPosY() == getAbsPosY()) return;

    switch (mode)
    {
    case shapePower: {
      float yL = previous->getAbsPosY();

      float yMin = (yL > getAbsPosY()) ? getAbsPosY() : yL;
      float yMax = (yL < getAbsPosY()) ? getAbsPosY() : yL;

      // y must not be yMax or yMin, else there will be a log(0). set y to be at least one pixel off of
      // these values.
      y = (y >= yMax) ? yMax - 1 : (y <= yMin) ? yMin + 1 : y;

      float test = yL - y / yL - getAbsPosY();
      curveCenterPosY.set((yL - y) / (yL - getAbsPosY()));
      break;
    }

    case shapeSine: {
      // For shapeSine the center point stays at the same position, while the sineOmega value is
      // still updated when moving the mouse in y-direction. Sine wave will always go through this
      // point. When omega is smaller 0.5, the sine will smoothly connect the points, if larger 0.5,
      // it will be increased in discrete steps, such that always n + 1/2 cycles lay in the interval
      // between the points.

      // TODO fix this.
      // if (sineOmega <= 0.5){
      //     sineOmega = sineOmegaPrevious * pow(0.5, (float)(y - getCurveCenterAbsPosY()) / 50);
      //     sineOmega = (sineOmega < SHAPE_MIN_OMEGA) ? SHAPE_MIN_OMEGA : sineOmega;
      // } else {
      //     // TODO if shift or strg or smth is pressed, be continuous
      //     sineOmega = sineOmegaPrevious + ((getCurveCenterAbsPosY() - y) / 40);
      //     sineOmega -= (int32_t)sineOmega % 2;
      // }
      break;
    }
    }
  }

  void processLeftClick()
  {
    // Save the previous omega state.
    sineOmegaPrevious = sineOmega;
  }
};

// Deletes the given ShapePoint and connects this points previous and next point in the list.
static void deleteShapePoint(ShapePoint* point)
{
  if (point->previous == nullptr)
  {
    throw std::invalid_argument("Tried to delete the first ShapePoint which can not be deleted.");
  }
  if (point->next == nullptr)
  {
    throw std::invalid_argument("Tried to delete the last ShapePoint which can not be deleted.");
  }

  // Link the new neighboring points together.
  ShapePoint* previous = point->previous;
  ShapePoint* next = point->next;

  previous->next = next;
  next->previous = previous;

  delete point;
}

// Inserts the ShapePoint *point into the linked list before *next.
static void insertPointBefore(ShapePoint* next, ShapePoint* point)
{
  if (next->previous == nullptr)
  {
    throw std::invalid_argument("Tried to insert shapePoint before first point, which is not allowed.");
  }

  ShapePoint* previous = next->previous;

  previous->next = point;
  point->previous = previous;

  point->next = next;
  next->previous = point;
}

ShapeEditor::ShapeEditor(int shapeEditorIndex) : index(shapeEditorIndex) {}

void ShapeEditor::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  layout.setCoordinates(rect, inGUIWidth, inGUIHeight);

  // There is a special ShapePoint at x=0, y=0, which can not be moved and is not displayed
  // to assure f(0) = 0. The last ShapePoint at x=1 may be moved, but only in y-direction.
  // None of these points can be removed to assure that the function is always well defined
  // on the interval [0, 1].
  shapePoints = new ShapePoint(0., 0., layout.editorRect, index);
  shapePoints->next = new ShapePoint(1., 1., layout.editorRect, index);

  shapePoints->next->previous = shapePoints;
}

ShapeEditor::~ShapeEditor()
{
  ShapePoint* current = shapePoints;
  ShapePoint* next;

  while (current != nullptr)
  {
    next = current->next;
    delete current;
    current = next;
  }
}

ShapePoint* ShapeEditor::getClosestPoint(float x, float y, float minimumDistance)
{
  // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.

  // TODO i do not like the implementation using the internal currentEditMode to communicate if point
  // or curve center was closer at all. Maybe this can be replaced by an optional argument of a pointer to a
  // editMode object.

  // The distance to the closest point, initialized as an arbitrary big number.
  float closestDistance = 10E6;

  // Pointer to the closest ShapePoint.
  ShapePoint* closestPoint;

  // Distance to the closest point.
  float distance;

  for (ShapePoint* point = shapePoints->next; point != nullptr; point = point->next)
  {
    distance = (point->getAbsPosX() - x) * (point->getAbsPosX() - x) + (point->getAbsPosY() - y) * (point->getAbsPosY() - y);
    if (distance < closestDistance)
    {
      closestDistance = distance;
      closestPoint = point;
      currentEditMode = position;
    }

    distance = (point->getCurveCenterAbsPosX() - x) * (point->getCurveCenterAbsPosX() - x) + (point->getCurveCenterAbsPosY() - y) * (point->getCurveCenterAbsPosY() - y);
    if (distance < closestDistance)
    {
      closestDistance = distance;
      closestPoint = point;
      currentEditMode = curveCenter;
    }
  }

  if (closestDistance <= minimumDistance)
  {
    return closestPoint;
  }
  else
  {
    currentEditMode = none;
    return nullptr;
  }
}

void ShapeEditor::deleteSelectedPoint()
{
  deleteShapePoint(rightClicked);
  deletedPoint = rightClicked;
  rightClicked = nullptr;
}

ShapePoint* ShapeEditor::getDeletedPoint()
{
  ShapePoint* deleted = deletedPoint;
  deletedPoint = nullptr;
  return deleted;
}

void ShapeEditor::processLeftClick(float x, float y)
{
  currentlyDragging = getClosestPoint(x, y);

  if (currentlyDragging != nullptr)
  {
    currentlyDragging->processLeftClick();
  }
}

void ShapeEditor::processDoubleClick(float x, float y)
{
  ShapePoint* closestPoint = getClosestPoint(x, y);

  if (closestPoint == nullptr) return;

  // Delete point if double click was performed close to point.
  if (currentEditMode == position && closestPoint->next != nullptr)
  {
    deleteShapePoint(closestPoint);
    deletedPoint = closestPoint;
    return;
  }

  // Reset interpolation shape if double click was performed close to curve center.
  else if (currentEditMode == curveCenter)
  {
    if (closestPoint->mode == shapePower)
    {
      closestPoint->curveCenterPosY.set(0.5);
    }
    else if (closestPoint->mode == shapeSine)
    {
      closestPoint->sineOmega = 0.5;
      closestPoint->sineOmegaPrevious = 0.5;
    }
    deletedPoint = nullptr;
    return;
  }
}

bool ShapeEditor::processRightClick(float x, float y)
{
  ShapePoint* closestPoint = getClosestPoint(x, y);

  // If not rightclicked in proximity of point, add one.
  if (closestPoint == nullptr)
  {
    ShapePoint* insertBefore = shapePoints->next;
    while (insertBefore != nullptr)
    {
      if (insertBefore->getAbsPosX() >= x)
      {
        break;
      }
      insertBefore = insertBefore->next;
    }

    float newPointX = (x - layout.editorRect.L) / layout.editorRect.W();
    float newPointY = (layout.editorRect.B - y) / layout.editorRect.H();

    ShapePoint* newPoint = new ShapePoint(newPointX, newPointY, layout.editorRect, index);
    insertPointBefore(insertBefore, newPoint);

    // Set the point as currently dragged point so the user can immediately adjust
    // the position after adding the point.
    currentlyDragging = newPoint;
    currentEditMode = position;
  }

  else
  {
    // You can not drag using the right mouse button.
    // Only exception is after creating a new ShapePoint (see above).
    currentlyDragging = nullptr;

    // If rightclicked on point, show shape menu to change function of curve segment.
    if (currentEditMode == position)
    {
      rightClicked = closestPoint;
      return true;
    }

    // If rightclicked on curve center, reset interpolation shape.
    if (currentEditMode == curveCenter)
    {
      if (closestPoint->mode == shapePower)
      {
        closestPoint->curveCenterPosY.set(0.5);
      }
      else if (closestPoint->mode == shapeSine)
      {
        closestPoint->sineOmega = 0.5;
        closestPoint->sineOmegaPrevious = 0.5;
      }
    }
  }
  return false;
}

void ShapeEditor::setInterpolationMode(Shapes shape)
{
  rightClicked->mode = shape;
  rightClicked = nullptr;
}

void ShapeEditor::processMouseDrag(float x, float y)
{
  if (!currentlyDragging) return;

  if (currentEditMode == position)
  {
    // The rightmost point must not move in x-direction.
    if (currentlyDragging->next == nullptr)
    {
      x = layout.editorRect.R;
      currentlyDragging->updatePositionAbsolute(x, y);
      return;
    }

    int32_t xLowerLim = currentlyDragging->previous->getAbsPosX();
    int32_t xUpperLim = currentlyDragging->next->getAbsPosX();

    x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
    y = (y > layout.editorRect.B) ? layout.editorRect.B : (y < layout.editorRect.T) ? layout.editorRect.T : y;

    currentlyDragging->updatePositionAbsolute(x, y);
  }

  else if (currentEditMode == curveCenter)
  {
    currentlyDragging->updateCurveCenter(x, y);
  }
}

const float ShapeEditor::forward(float input, double beatPosition, double secondsPlayed)
{
  // Catching this case is important because the function might return non-zero values for steep curves
  // due to quantization errors, which would result in an DC offset even when no input audio is given.
  if (input == 0) return 0;

  float out;

  // Absolute value of input is processed, information about sign is saved to flip output after computing.
  bool flipOutput = input < 0;
  input = (input < 0) ? -input : input;

  input = (input > 1) ? 1 : input;

  ShapePoint* point = shapePoints->next;

  // Find the curve segment concerned by the input.
  while (point->getPosX(beatPosition, secondsPlayed) < input)
  {
    point = point->next;
  }

  switch (point->mode)
  {
  case shapePower: {
    float xL = point->previous->getPosX(beatPosition, secondsPlayed);
    float yL = point->previous->getPosY(beatPosition, secondsPlayed);
    float x = point->getPosX(beatPosition, secondsPlayed);

    // Compute relative x-position inside the curve segment, relative "height" of the curve segment and power
    // corresponding to the current curve cenetr position.
    float relX = (x == xL) ? xL : (input - xL) / (x - xL);
    float segmentYExtent = (point->getPosY(beatPosition, secondsPlayed) - yL);
    float power = getPowerFromPosY(point->curveCenterPosY.get(beatPosition, secondsPlayed));

    if (power > 0)
    {
      out = yL + pow(relX, power) * segmentYExtent;
    }
    else
    {
      out = point->getPosY(beatPosition, secondsPlayed) - pow(1 - relX, -power) * segmentYExtent;
    }
    break;
  }
  case shapeSine:
    out = 1.;
    break;
  }
  return flipOutput ? -out : out;
}

// TODO how to do this in iPlug
// Saves the ShapeEditor state to the clap_ostream.
// First saves the number of ShapePoints as an int and then saves:
//  - (float)   point x-position
//  - (float)   point y-position
//  - (float)   point curve center y-position
//  - (Shapes)  point interpolation mode
//  - (float)   point omega value
//
// for every ShapePoint. These values describe the ShapeEditor state entirely.
//bool ShapeEditor::saveState(const clap_ostream_t* stream)
//{
//  ShapePoint* point = shapePoints->next;
//
//  int numberPoints = 0;
//  while (point != nullptr)
//  {
//    point = point->next;
//    numberPoints++;
//  }
//
//  if (stream->write(stream, &numberPoints, sizeof(int)) != sizeof(int))
//    return false;
//
//  point = shapePoints->next;
//  while (point != nullptr)
//  {
//    // Save ModulatedParameter base values.
//    if (stream->write(stream, point->posX.getBaseAddress(), sizeof(float)) != sizeof(float))
//      return false;
//    if (stream->write(stream, point->posY.getBaseAddress(), sizeof(float)) != sizeof(float))
//      return false;
//    if (stream->write(stream, point->curveCenterPosY.getBaseAddress(), sizeof(float)) != sizeof(float))
//      return false;
//
//    // Save interpolation mode and interpolation parameters.
//    if (stream->write(stream, &point->mode, sizeof(point->mode)) != sizeof(point->mode))
//      return false;
//    if (stream->write(stream, &point->sineOmega, sizeof(point->sineOmega)) != sizeof(point->sineOmega))
//      return false;
//
//    point = point->next;
//  }
//
//  return true;
//}

// Loads a ShapeEditor state from a clap_istream_t. The state is defined by the state of every
// ShapePoint, which is given by:
//  - (float) posX
//  - (float) posY
//  - (float) curceCenterY
//  - (Shapes) mode
//  - (float) sineOmega
//
// The first element in the stream must be the number of ShapePoints to be loaded. It is
// expected to be called in UDShaper::loadState() after the version number was loaded.
//bool ShapeEditor::loadState(const clap_istream_t* stream, int version[3])
//{
//  if (version[0] == 1 && version[1] == 0 && version[2] == 0)
//  {
//    int numberPoints;
//    if (stream->read(stream, &numberPoints, sizeof(int)) != sizeof(int))
//      return false;
//
//    ShapePoint* last = shapePoints->next;
//
//    for (int i = 0; i < numberPoints; i++)
//    {
//      float posX, posY, curveCenterY, sineOmega;
//      Shapes mode;
//      int numberLinks;
//
//      if (stream->read(stream, &posX, sizeof(posX)) != sizeof(posX))
//        return false;
//      if (stream->read(stream, &posY, sizeof(posY)) != sizeof(posY))
//        return false;
//      if (stream->read(stream, &curveCenterY, sizeof(curveCenterY)) != sizeof(curveCenterY))
//        return false;
//      if (stream->read(stream, &mode, sizeof(mode)) != sizeof(mode))
//        return false;
//      if (stream->read(stream, &sineOmega, sizeof(sineOmega)) != sizeof(sineOmega))
//        return false;
//
//      float power = getPowerFromPosY(curveCenterY);
//
//      if (i != numberPoints - 1)
//      {
//        ShapePoint* newPoint = new ShapePoint(posX, posY, layout.editorXYXY, power, sineOmega, mode);
//        insertPointBefore(last, newPoint);
//      }
//      else
//      {
//        // The last point is created when ShapeEditor is initialized, so it does not have to be added.
//        // Only set the attribute values.
//        last->posY.set(posY);
//        last->curveCenterPosY.set(curveCenterY);
//        last->mode = mode;
//        last->sineOmega = sineOmega;
//      }
//    }
//    return true;
//  }
//  else
//  {
//    // TODO if version is not known one of these things should happen:
//    //  - If a preset was loaded, it means probably that the preset was saved from a more
//    //    recent version of UDShaper and can therefore not be loaded correctly.
//    //    There should be a message to warn the user.
//    //  - If it happens from copying/ moving by the host, throw an exception probably?
//    return false;
//  }
//  return true;
//}

const void ShapeEditor::attachUI(IGraphics* g) {
  assert(!layout.fullRect.Empty());

  g->AttachControl(new FillRectangle(layout.innerRect, UDS_ORANGE));
  g->AttachControl(new DrawFrame(layout.editorRect, UDS_WHITE, RELATIVE_FRAME_WIDTH_EDITOR*layout.GUIWidth));
  g->AttachControl(new ShapeEditorControl(layout.innerRect, layout.editorRect, this, 256));
}

ShapeEditorControl::ShapeEditorControl(const IRECT& bounds, const IRECT& editorBounds, ShapeEditor* shapeEditor, int numPoints, bool useLayer)
  : IControl(bounds)
  , IVectorBase(DEFAULT_STYLE)
  , mUseLayer(useLayer)
  , editorRect(editorBounds)
{
  editor = shapeEditor;
  mPoints.resize(numPoints);

  AttachIControl(this, "");

  if (mLayer)
    mLayer->Invalidate();
  SetDirty(false);

  menu.AddItem("Delete", 0);
  menu.AddSeparator(1);
  menu.AddItem("Power", 2);
  // TODO add this once sinusoidal interpolation is implemented.
  //menu.AddItem("Sine", 3);
}

void ShapeEditorControl::setEditor(ShapeEditor* newEditor)
{
  editor = newEditor;
  SetDirty(true);
}

void ShapeEditorControl::Draw(IGraphics& g)
{
  if (editor == nullptr) return;

  float hdiv = editorRect.W() / static_cast<float>(mHorizontalDivisions);
  float vdiv = editorRect.H() / static_cast<float>(mVerticalDivisions);

  auto drawFunc = [&]() {
    // TODO Segments between very close points are not rendered
    // + The curve can overshoot points on very steep peaks.
    // Set an additional samplepoint at every ShapePoint to prevent this?

    g.DrawGrid(UDS_WHITE, editorRect, hdiv, vdiv, &gridBlend);

    // Draw the graph of the shaping function.
    for (int i = 0; i < mPoints.size(); i++)
    {
      float v = editor->forward(static_cast<float>(i) / static_cast<float>(mPoints.size() - 1));
      v = (v - mMin) / (mMax - mMin);
      mPoints.at(i) = v;
    }

    g.DrawData(UDS_WHITE, editorRect, mPoints.data(), static_cast<int>(mPoints.size()), nullptr, &mBlend, mTrackSize);

    // Draw the ShapePoints on top of the graph.
    ShapePoint* point = editor->shapePoints->next;
    while (point != nullptr)
    {
      float posX = point->getAbsPosX();
      float posY = point->getAbsPosY();
      float curveCenterPosX = point->getCurveCenterAbsPosX();
      float curveCenterPosY = point->getCurveCenterAbsPosY();
      g.FillCircle(UDS_WHITE, posX, posY, 0.4 * RELATIVE_POINT_SIZE * editor->layout.GUIWidth);
      g.FillCircle(UDS_WHITE, curveCenterPosX, curveCenterPosY, 0.4 * RELATIVE_POINT_SIZE_SMALL * editor->layout.GUIWidth);
      point = point->next;
    }
  };

  if (mUseLayer)
  {
    if (!g.CheckLayer(mLayer))
    {
      g.StartLayer(this, mRECT);
      drawFunc();
      mLayer = g.EndLayer();
    }

    g.DrawLayer(mLayer, &mBlend);
  }
  else
    drawFunc();
}

void ShapeEditorControl::OnResize()
{
  SetTargetRECT(MakeRects(mRECT));
  SetDirty(false);
}

void ShapeEditorControl::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  if (mod.L) editor->processLeftClick(x, y);
  if (mod.R)
  {
    if (editor->processRightClick(x, y))
    {
      GetUI()->CreatePopupMenu(*this, menu, IRECT(x, y, x, y));
    }
  }
  SetDirty(true);
}

void ShapeEditorControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  editor->processMouseDrag(x, y);
  SetDirty(true);
}

void ShapeEditorControl::OnMouseDblClick(float x, float y, const IMouseMod& mod)
{
  if (mod.L)
  {
    editor->processDoubleClick(x, y);
    SetDirty(true);
  }
}

void ShapeEditorControl::OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx)
{
  if (pSelectedMenu != nullptr)
  {
    int item = pSelectedMenu->GetChosenItemIdx();
    if (item == 0)
    {
      editor->deleteSelectedPoint();
    }
    else if (item == 2)
    {
      editor->setInterpolationMode(shapePower);
    }
    else if (item == 3)
    {
      editor->setInterpolationMode(shapeSine);
    }
  }
}