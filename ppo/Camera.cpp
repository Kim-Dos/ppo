#include "Camera.h"
#include "Player.h"

Camera::Camera()
{
    SetLens(0.25f * MathHelper::Pi, 1.0f, 1.0f, 2000.0f);
}

Camera::~Camera()
{
}

XMVECTOR Camera::GetPosition() const
{
	return XMLoadFloat3(&mPosition);
}

XMFLOAT3 Camera::GetPosition3f() const
{
	//return mPosition;
	return mPosition;
}

//XMFLOAT3 Camera::GetRotatePosition3f() const
//{
//	XMMATRIX rotationMatrix(
//		mRight.x, mRight.y, mRight.z, 0.0f,
//		mUp.x, mUp.y, mUp.z, 0.0f,
//		mLook.x, mLook.y, mLook.z, 0.0f,
//		0.0f, 0.0f, 0.0f, 1.0f
//	);
//	XMFLOAT3 rotatePosition;
//	XMStoreFloat3(&rotatePosition, XMVector3TransformCoord(XMLoadFloat3(&mOffsetPosition), rotationMatrix));
//	rotatePosition.x += mPosition.x;
//	rotatePosition.y += mPosition.y;
//	rotatePosition.z += mPosition.z;
//
//	return rotatePosition;
//}

void Camera::SetPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
	mViewDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
	mPosition = Vector3::Add(v, mOffsetPosition);
	mViewDirty = true;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::SetLens(float fovY, float aspect)
{
	mFovY = fovY;
	mAspect = aspect;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);

	mViewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);

	mViewDirty = true;
}

void Camera::SetOffset(XMFLOAT3 position, float pitch, float yaw, float roll)
{
	mOffsetPosition = position;
	/*
	XMStoreFloat4x4(&mOffsetMat, 
		XMMatrixTranslation(position.x, position.y, position.z) * 
		XMMatrixRotationRollPitchYaw(XMConvertToRadians(pitch), XMConvertToRadians(yaw), XMConvertToRadians(roll)));
	*/
	mViewDirty = true;
}

XMMATRIX Camera::GetView() const
{
	// 카메라가 이동하거나 회전한 이후 업데이트되지 않은 경우
	assert(!mViewDirty);
	return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj() const
{
	return XMLoadFloat4x4(&mProj);
}

XMFLOAT4X4 Camera::GetView4x4f() const
{
	assert(!mViewDirty);
	return mView;
}

XMFLOAT4X4 Camera::GetProj4x4f() const
{
	return mProj;
}

void Camera::Strafe(float d)
{
	XMFLOAT3 right = XMFLOAT3(mRight.x, 0.0f, mRight.z);

	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&right);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

	mViewDirty = true;
}

void Camera::Walk(float d)
{
	XMFLOAT3 look = XMFLOAT3(mLook.x, 0.0f, mLook.z);

	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&look);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

	mViewDirty = true;
}

void Camera::Up(float d)
{
	XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&up);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(l, s, p));

	mViewDirty = true;
}

void Camera::Pitch(float radian)
{
	if (fabs(mCurrentPitch + radian) > mMaxPitch) {
		radian = (radian > 0) ? XMConvertToRadians(mMaxPitch - mCurrentPitch) : -XMConvertToRadians(mMaxPitch + mCurrentPitch);
		mCurrentPitch = (mCurrentPitch + radian> 0) ? mMaxPitch : -mMaxPitch;
	}

	mCurrentPitch += XMConvertToDegrees(radian);

	// up, look 벡터를 right 벡터에 대해 회전
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), radian);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::RotateY(float radian)
{
	// up, look, right를 월드 공간 Y축에 대해 회전
	XMMATRIX R = XMMatrixRotationY(radian);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
	if (mViewDirty)
	{
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPosition);

		// 카메라의 벡터를 서로 직교인 단위벡터가 되게 한다.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		R = XMVector3Cross(U, L);

		// 시야 행렬의 성분들을 채운다.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		mView = XMFLOAT4X4(
			mRight.x, mUp.x, mLook.x, 0.0f,
			mRight.y, mUp.y, mLook.y, 0.0f,
			mRight.z, mUp.z, mLook.z, 0.0f,
			x, y, z, 1.0f
		);

		mViewDirty = false;
	}
}

void Camera::Move(const float& deltatime)
{
		// 최대 속도 제한
	float Speed = Vector3::Length(mVelocity);
	if (Speed > maxSpeed) {
		mVelocity.x *= maxSpeed / Speed;
		mVelocity.y *= maxSpeed / Speed;
		mVelocity.z *= maxSpeed / Speed;
	}

	// 마찰
	XMFLOAT3 friction;
	XMStoreFloat3(&friction, -XMVector3Normalize(XMVectorSet(mVelocity.x, 0.0f, mVelocity.z, 0.0f)) * mFriction * deltatime);
	mVelocity.x = (mVelocity.x >= 0.0f) ? max(0.0f, mVelocity.x + friction.x) : min(0.0f, mVelocity.x + friction.x);
	mVelocity.x = (mVelocity.y >= 0.0f) ? max(0.0f, mVelocity.y + friction.y) : min(0.0f, mVelocity.y + friction.y);
	mVelocity.z = (mVelocity.z >= 0.0f) ? max(0.0f, mVelocity.z + friction.z) : min(0.0f, mVelocity.z + friction.z);

	// 위치 변환
	SetPosition(Vector3::Add(GetPosition3f(), Vector3::ScalarProduct(mVelocity, deltatime, false)));
}

void Camera::SetPlayerDirections(Player* p) {

	SetPosition(XMFLOAT3(0.0f, 100.0f, 20.0f));

	XMVECTOR cameraLook = XMVector3TransformNormal(XMLoadFloat3(&p->GetLook()), XMMatrixRotationAxis(XMLoadFloat3(&p->GetRight()), p->GetPitch()));

	XMVECTOR playerPosition = XMLoadFloat3(&p->GetPosition()) + XMLoadFloat3(&GetPosition3f());
	XMVECTOR cameraPosition = playerPosition - cameraLook * 500.f; // distance는 카메라와 플레이어 사이의 거리

	XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	LookAt(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	UpdateViewMatrix();
}

