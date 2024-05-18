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

LobbyButton::LobbyButton(const POINT& cen, const SIZE& siz) noexcept
	: Button(cen ,siz)
{
}

void LobbyButton::ButtonAction()
{

	printf("");
	int a = 0;
	a++;
	a += 2;

	int b = a + 10;

}
