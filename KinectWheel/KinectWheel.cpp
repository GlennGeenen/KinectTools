// KinectWheel.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "KinectWheel.h"

using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
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

KinectWheel::KinectWheel(): m_pKinectSensor(NULL), m_pBodyFrameReader(NULL), m_trackingId(0)
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

		if(_countof(ppBodies) > 0)
		{
			if(m_trackingId == 0)
			{
				m_trackingId = getTrackingId(BODY_COUNT, ppBodies);
			}

			if (SUCCEEDED(hr))
			{
				ProcessBody(BODY_COUNT, ppBodies);
			}

			for (int i = 0; i < _countof(ppBodies); ++i)
			{
				SafeRelease(ppBodies[i]);
			}
		}
		else 
		{
			m_trackingId = 0;
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
	float forward = 0.4f;
	float stop = 0.2f;

	if(mid.Z - rightHand.Z > 0 && mid.Z - leftHand.Z > 0
		&& rightHand.X - leftHand.X < forward)
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

    for (int i = 0; i < nBodyCount; ++i)
    {
        IBody* pBody = ppBodies[i];
		pBody->get_TrackingId(&trackingid);
		if (pBody && trackingid == m_trackingId)
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
					steerCar(joints[JointType_HandLeft].Position,
							joints[JointType_HandRight].Position,
							joints[JointType_SpineMid].Position);
				}
            }
        }
    }
}