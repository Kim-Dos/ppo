#include "Button.h"

Button::Button(const POINT& cen, const SIZE& siz) noexcept
	: center(cen), size(siz)
{
	activating = true;
}

bool Button::isActive() const
{
	return activating;
}

bool Button::isInButton(int x, int y) const
{
	RECT DoingButton = { center.x - size.cx,center.y - size.cy,center.x + size.cx,center.y + size.cy };

	return PtInRect(&DoingButton, { x,y });
}


//-----------Lobby Buttons--------------------

LobbyButton::LobbyButton(const POINT& cen, const SIZE& siz, const int& textureID) noexcept
	: Button(cen ,siz), texID(textureID)
{
}


ButtonPack& LobbyButton::ButtonAction()
{

	switch (texID)
	{
	case CL_LOGIN:
		LoginActing();
		break;
	case CL_LOGOUT:
		LogOutActing();
		break;
	case CL_QUICK_MATCHING:
		QuickMatchingActing();
		break;
	case CL_ENTER_ROOM_CODE:
		EnterRoomCode();
		break;
	case CL_START_GAME:
		GameStart();
		break;
	default:
		printf("Other Texture\n");
		exit(-1);
		break;
	}
	

}

ButtonPack& LobbyButton::LoginActing()
{
	CLLobbyLogin packet;

	ButtonPack* p = new ButtonPack{ &packet, sizeof(CLLobbyLogin) };
	return *p;
}

ButtonPack& LobbyButton::LogOutActing()
{
	CLLobbyLogOut packet;

	ButtonPack* p = new ButtonPack{ &packet, sizeof(CLLobbyLogOut) };
	return *p;
}

ButtonPack& LobbyButton::QuickMatchingActing()
{
	CLClickMatching packet;
	ButtonPack* p = new ButtonPack{ &packet, sizeof(CLClickMatching) };
	return *p;
}

ButtonPack& LobbyButton::EnterRoomCode()
{
	CLEnterRoomCode packet;
	ButtonPack* p = new ButtonPack{ &packet, sizeof(CLEnterRoomCode) };
	return *p;
}

ButtonPack& LobbyButton::GameStart()
{
	CLStartGame packet;
	ButtonPack* p = new ButtonPack{ &packet, sizeof(CLStartGame) };
	return *p;
}
