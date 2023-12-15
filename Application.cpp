#include "EntityFrame.h"

class EntityApp : public wxApp
{
	public:
	virtual bool OnInit() wxOVERRIDE
	{
		if(!wxApp::OnInit())
			return false;

		EntityFrame* frame = new EntityFrame();
		frame->Show(true);
		return true;
	}
};

wxIMPLEMENT_APP(EntityApp);