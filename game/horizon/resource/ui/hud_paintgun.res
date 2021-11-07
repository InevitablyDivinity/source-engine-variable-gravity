"resource/ui/hud_paintgun.res"
{
	HudPaintGun
	{
		fieldName		HudPaintGun	// name of the control element
		xpos			50					// x position
		ypos			50					// y position
		wide			900					// width in pixels
		tall			54					// height in pixels
		visible			1					// it's visible...
		enabled			1					// ...and enabled
	}

	CurrentPaintLabel
	{
		ControlName		Label				// control class 
		fieldName		CurrentPaintLabel	// name of the control element
		xpos			2					// x position
		ypos			0					// y position
		wide			80					// width in pixels
		tall			20					// height in pixels
		visible			1					// it's visible...
		enabled			1					// ...and enabled
		textAlignment	west				// left text alignment
		font			DefaultSmall
	}
	
	NextPaintLabel
	{
		ControlName		Label				// control class 
		fieldName		NextPaintLabel		// name of the control element
		xpos			2					// x position
		ypos			10					// y position
		wide			80					// width in pixels
		tall			20					// height in pixels
		visible			1					// it's visible...
		enabled			1					// ...and enabled
		textAlignment	west				// left text alignment
		font			DefaultSmall
	}
	
	PreviousPaintLabel
	{
		ControlName		Label				// control class 
		fieldName		PreviousPaintLabel	// name of the control element
		xpos			2					// x position
		ypos			-20					// y position
		visible			1					// it's visible...
		enabled			1					// ...and enabled
		textAlignment	west				// left text alignment
		font			DefaultSmall
	}
}