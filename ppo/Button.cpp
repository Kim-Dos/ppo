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
	case CS_LOGIN:
		CSLobbyLogin packet;
		ButtonPack* p = new ButtonPack{ &packet, sizeof(CSLobbyLogin) };
		return *p;
	case CS_LOGOUT:
		CSLobbyLogOut packet;
		ButtonPack* p = new ButtonPack{ &packet, sizeof(CSLobbyLogOut) };
		return *p;
	case CS_QUICK_MATCHING:
		CSClickMatching packet;
		ButtonPack* p = new ButtonPack{ &packet, sizeof(CSClickMatching) };
		return *p;
	case CS_ENTER_ROOM_CODE:
		CSEnterRoomCode packet;
		ButtonPack* p = new ButtonPack{ &packet, sizeof(CSEnterRoomCode) };
		return *p;
	case CS_START_GAME:
		CSStartGame packet;
		ButtonPack* p = new ButtonPack{ &packet, sizeof(CSStartGame) };
		return *p;
	default:
		printf("Other Texture\n");
		exit(-1);
		break;
	}
	

}

ButtonPack& LobbyButton::LoginActing()
{
	CSLobbyLogin packet;

	ButtonPack* p = new ButtonPack{ &packet, sizeof(CSLobbyLogin) };
	return *p;
}

ButtonPack& LobbyButton::LogOutActing()
{
	CSLobbyLogOut packet;

	ButtonPack* p = new ButtonPack{ &packet, sizeof(CSLobbyLogOut) };
	return *p;
}

ButtonPack& LobbyButton::QuickMatchingActing()
{
	CSClickMatching packet;
	ButtonPack* p = new ButtonPack{ &packet, sizeof(CSClickMatching) };
	return *p;
}

ButtonPack& LobbyButton::EnterRoomCode()
{
	CSEnterRoomCode packet;
	ButtonPack* p = new ButtonPack{ &packet, sizeof(CSEnterRoomCode) };
	return *p;
}

ButtonPack& LobbyButton::GameStart()
{
	CSStartGame packet;
	ButtonPack* p = new ButtonPack{ &packet, sizeof(CSStartGame) };
	return *p;
}
