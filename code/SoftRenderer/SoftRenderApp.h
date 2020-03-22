/**
  @file SoftRenderApp.h
  
  @author yikaiming

  ������־ history
  ver:1.0
   
 */

#ifndef SoftRenderApp_h__
#define SoftRenderApp_h__

#include "SrAppFramework.h"

typedef std::vector<IRenderer*> SrRendererList;

class SoftRenderApp 
{
public:
	SoftRenderApp(void);
	virtual ~SoftRenderApp(void);
	bool Init(int width, int height);
	void Destroy();
	void Run();

	bool Update();
	
	void RegisterTask(SrAppFramework* task);
	void UnRegisterTasks();

	bool InitRenderers();
	void ShutdownRenderers();

	void SendEvent(const char* event, const char* argument);

private:
	SrApps m_tasks;
	IRenderer* m_renderer;
	uint32 m_currRendererIndex;

	void LoadShaderList();

	int m_curr_task;

};

#endif // SoftRenderApp_h__