
#include "stdafx.h"

typedef enum e_ButtonPress
{
    ButtonUp = 0,
	ButtonRight = 1,
    ButtonDown = 2,
    ButtonLeft = 3
} ButtonPress;

class KinectWheel
{
public:

	KinectWheel();
    ~KinectWheel();

	void Update();

private:

	// Private Members
	IKinectSensor* m_pKinectSensor;
	IBodyFrameReader* m_pBodyFrameReader;
	UINT64 m_trackingId;
	bool keysPressed[4];

	// Private Methods
	void InitializeDefaultSensor();
	UINT64 getTrackingId(int nBodyCount, IBody** ppBodies);
	void ProcessBody(int nBodyCount, IBody** ppBodies);
	void steerCar(const CameraSpacePoint& leftHand, const CameraSpacePoint& rightHand, const CameraSpacePoint& mid);
	void pressButton(ButtonPress button, bool startPress = true);

};