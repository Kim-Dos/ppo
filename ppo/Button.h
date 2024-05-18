#include "stdafx.h"



class Button
{
public:

	Button(const POINT& cen, const SIZE& siz) noexcept;
	~Button() {}

	bool isActive() const;

	bool isInButton(int x, int y) const;
	
	virtual void ButtonAction() = 0;
	

private:
	POINT center;
	SIZE  size;
	bool activating;
};

class LobbyButton : public Button
{
public:
	LobbyButton(const POINT& cen, const SIZE& siz) noexcept;

	virtual void ButtonAction() override;

private:
	int texID;
};