// KinectWheel.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <conio.h>
#include "KinectWheel.h"
#include "vjoyinterface.h"

using namespace std;

#define HID_USAGE_X		0x30
#define HID_USAGE_Y		0x31

UINT iInterface=1;	

int _tmain(int argc, _TCHAR* argv[])
{
	cout << "Do you want to use the vJoy mode (Y/N)\n";

	char c;
	cin >> c;

	if(c == 'Y')
	{
		cout << "Started in vJoy Mode.\n";
		if (argc>1 && wcslen(argv[1]))
		{
			sscanf_s((char *)(argv[1]), "%d", &iInterface);
		}
		AcquireVJD(iInterface);
		ResetVJD(iInterface);
	}
	else 
	{
		cout << "Started in Keyboard Mode.\n";
	}

	KinectWheel* kinectWheel = new KinectWheel();

	const int TICKS_PER_SECOND = 30;
	const int SKIP_TICKS = 1000 / TICKS_PER_SECOND;
	const int MAX_FRAMESKIP = 10;
	DWORD next_game_tick = GetTickCount();
	int loops = 0;

	while( true )
	{
		loops = 0;
		while( GetTickCount() > next_game_tick && loops < MAX_FRAMESKIP)
		{
			kinectWheel->Update();
			next_game_tick += SKIP_TICKS;
			++loops;
		}
	}

	return 0;
}

KinectWheel::KinectWheel(bool usejoystick): m_useJoystick(usejoystick), m_pKinectSensor(NULL), m_pBodyFrameReader(NULL), m_trackingId(0)
{
	InitializeDefaultSensor();
}

KinectWheel::~KinectWheel()
{
	SafeRelease(m_pKinectSensor);
	SafeRelease(m_pBodyFrameReader);
}

void KinectWheel::Update()
{
    if (!m_pBodyFrameReader)
    {
        return;
    }

    IBodyFrame* pBodyFrame = NULL;

    HRESULT hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);

    if (SUCCEEDED(hr))
    {
        IBody* ppBodies[BODY_COUNT] = {0};
		hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);

		if (SUCCEEDED(hr))
		{
			if(m_trackingId == 0)
			{
				m_trackingId = getTrackingId(BODY_COUNT, ppBodies);
			}
			ProcessBody(BODY_COUNT, ppBodies);
		}

		for (int i = 0; i < _countof(ppBodies); ++i)
		{
			SafeRelease(ppBodies[i]);
		}
    }

    SafeRelease(pBodyFrame);
}

void KinectWheel::InitializeDefaultSensor()
{
    HRESULT hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
		cout << "Kinect Failed to initialize.\n";
        return;
    }

    if (m_pKinectSensor)
    {
        IBodyFrameSource* pBodyFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_BodyFrameSource(&pBodyFrameSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = pBodyFrameSource->OpenReader(&m_pBodyFrameReader);
        }

        SafeRelease(pBodyFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
		cout << "Kinect Failed to initialize.\n";
    }
	else
	{
		cout << "Kinect initialized.\n";
	}
}

void KinectWheel::pressButton(ButtonPress button, bool startPress)
{
	WORD vkey;

	if(button == ButtonUp)
	{
		// W
		vkey = 0x11;
	}
	else if(button == ButtonDown)
	{
		// S
		vkey = 0x1F;
	}
	else if(button == ButtonLeft)
	{
		// A
		vkey = 0x1E;
	}
	else if(button == ButtonRight)
	{
		// D
		vkey = 0x20;
	}

	keysPressed[button] = startPress;

	INPUT input[1];

	input[0].type = INPUT_KEYBOARD;
	input[0].ki.time = 0;
	input[0].ki.dwExtraInfo = 0;
	input[0].ki.wScan = vkey;
	if(startPress)
	{
		input[0].ki.dwFlags = KEYEVENTF_SCANCODE;
	}
	else
	{
		input[0].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
	}

	SendInput(1, input, sizeof(INPUT));
}

void KinectWheel::steerCar(const CameraSpacePoint& leftHand, const CameraSpacePoint& rightHand, const CameraSpacePoint& mid)
{
	if(m_useJoystick)
	{
		float offsetZ = 0.15f;

		if(mid.Z > rightHand.Z && mid.Z > leftHand.Z
			&& rightHand.X - leftHand.X < 0.5f
			&& rightHand.Y > mid.Y && leftHand.Y > mid.Y)
		{
			float steerY = (rightHand.Z + leftHand.Z) * 0.5f;
			
			// Scale input from 0 to 33000

			// 2.85f is used to have maximum speed when holding your hands 0.5m in front of your body
			// An offset of 0.15m is used because you can't hold your hands in your body

			steerY = (mid.Z - steerY - offsetZ) * 2.85f;

			// The value 33000 represents going reverse full speed
			steerY = 1 - steerY;
			steerY *= 33000.0f;
			if(steerY > 33000.0f)
			{
				steerY = 33000.0f;
			}
			if (steerY < 0.0f)
			{
				steerY = 0.0f;
			}

			float deltaX = rightHand.X - leftHand.X;
			float deltaY = rightHand.Y - leftHand.Y;

			// Get angle between 0 and 180
			float steerX = atan2(deltaX,deltaY) * 180 / 3.14159265358979f;

			// Scale input from 0 to 33000
			steerX *= 183.333f;
			if(steerX < 0.0f)
			{
				steerX = 0.0f;
			}
			if(steerX > 33000.0f)
			{
				steerX = 33000.0f;
			}

			// Filter inacurate angle
			if(steerX < 10000 && rightHand.Y < leftHand.Y
				|| steerX > 23000 && leftHand.Y < rightHand.Y)
			{
				steerX = 33000 - steerX;
			}

			SetAxis(steerX, iInterface, HID_USAGE_X);
			SetAxis(steerY, iInterface, HID_USAGE_Y);
		}
	}
	else 
	{
		float forward = 0.4f;
		float stop = 0.25f;

		if(mid.Z - rightHand.Z > 0 && mid.Z - leftHand.Z > 0
			&& rightHand.X - leftHand.X < forward
			&& rightHand.Y > mid.Y && leftHand.Y > mid.Y)
		{

			if(mid.Z - rightHand.Z > forward && mid.Z - leftHand.Z > forward)
			{
				pressButton(ButtonUp);
			}
			else if(keysPressed[ButtonUp])
			{
				pressButton(ButtonUp, false);
			}
		
			if(mid.Z - rightHand.Z < stop && mid.Z - leftHand.Z < stop)
			{
				pressButton(ButtonDown);
			}
			else if(keysPressed[ButtonDown])
			{
				pressButton(ButtonDown, false);
			}

			if(rightHand.Y - leftHand.Y > 0.1f)
			{
				pressButton(ButtonLeft);
			}
			else if(keysPressed[ButtonLeft])
			{
				pressButton(ButtonLeft, false);
			}

			if(leftHand.Y - rightHand.Y > 0.1f)
			{
				pressButton(ButtonRight);
			}
			else if(keysPressed[ButtonRight])
			{
				pressButton(ButtonRight, false);
			}
		}
	}
}

UINT64 KinectWheel::getTrackingId(int nBodyCount, IBody** ppBodies)
{
	float z = 999.0f;
	UINT64 trackingid = 0;

	for (int i = 0; i < nBodyCount; ++i)
	{
		IBody* pBody = ppBodies[i];
		if (pBody)
		{
			BOOLEAN bTracked = false;
			HRESULT hr = pBody->get_IsTracked(&bTracked);

			if (SUCCEEDED(hr) && bTracked)
			{
				Joint joints[JointType_Count];
				HandState leftHandState = HandState_Unknown;
				HandState rightHandState = HandState_Unknown;

				pBody->get_HandLeftState(&leftHandState);
				pBody->get_HandRightState(&rightHandState);

				hr = pBody->GetJoints(_countof(joints), joints);
				if (SUCCEEDED(hr))
				{
					if(joints[JointType_SpineMid].Position.Z < z)
					{
						z = joints[JointType_SpineMid].Position.Z;
						pBody->get_TrackingId(&trackingid);
					}
				}
			}
		}
	}
	return trackingid;
}

void KinectWheel::ProcessBody(int nBodyCount, IBody** ppBodies)
{
	UINT64 trackingid = 0;
	bool hasTrackingId = false;
    for (int i = 0; i < nBodyCount; ++i)
    {
        IBody* pBody = ppBodies[i];
		pBody->get_TrackingId(&trackingid);
		if (pBody && trackingid == m_trackingId)
        {
			hasTrackingId = true;

            BOOLEAN bTracked = false;
            HRESULT hr = pBody->get_IsTracked(&bTracked);

            if (SUCCEEDED(hr) && bTracked)
            {
                Joint joints[JointType_Count];
                HandState leftHandState = HandState_Unknown;
                HandState rightHandState = HandState_Unknown;

                pBody->get_HandLeftState(&leftHandState);
                pBody->get_HandRightState(&rightHandState);

                hr = pBody->GetJoints(_countof(joints), joints);
                if (SUCCEEDED(hr))
                {
					steerCar(joints[JointType_HandLeft].Position,
							joints[JointType_HandRight].Position,
							joints[JointType_SpineBase].Position);
				}
            }
        }
    }

	if(!hasTrackingId)
	{
		m_trackingId = 0;
	}
}