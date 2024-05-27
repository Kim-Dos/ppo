#include "stdafx.h"



class Button
{
public:

	Button(const POINT& cen, const SIZE& siz) noexcept;
	~Button() {}

	bool isActive() const;

	bool isInButton(int x, int y) const;
	
	virtual ButtonPack& ButtonAction() = 0;
	

private:
	POINT center;
	SIZE  size;
	bool activating;
};



class LobbyButton : public Button
{
public:
	LobbyButton(const POINT& cen, const SIZE& siz, const int& textureID) noexcept;

	virtual ButtonPack& ButtonAction() override;

	ButtonPack& LoginActing();
	ButtonPack& LogOutActing();
	ButtonPack& QuickMatchingActing();
	ButtonPack& EnterRoomCode();
	ButtonPack& GameStart(); 

private:
	int texID;
};