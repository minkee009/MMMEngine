#include "Player.h"
#include "MMMInput.h"
#include "MMMTime.h"

void MMMEngine::Player::Update()
{
	if (Input::GetKey(KeyCode::LeftArrow))
	{
		posX -= velocity * Time::GetDeltaTime();
	}
	if (Input::GetKey(KeyCode::RightArrow))
	{
		posX += velocity * Time::GetDeltaTime();
	}
	if (Input::GetKey(KeyCode::UpArrow))
	{
		posY += velocity * Time::GetDeltaTime();
	}
	if (Input::GetKey(KeyCode::DownArrow))
	{
		posY -= velocity * Time::GetDeltaTime();
	}
}