#ifndef _GEVENTTARGETTHREAD_H_
#define _GEVENTTARGETTHREAD_H_

#include "GThread.h"
#include "GMutex.h"
#include "GThreadEvent.h"

#define PostThreadEvent GEventSinkMap::Dispatch.PostEvent

class LgiClass GEventSinkMap : public GMutex
{
protected:
	GHashTbl<int,GEventSinkI*> ToPtr;
	GHashTbl<void*,int> ToHnd;

public:
	static GEventSinkMap Dispatch;

	GEventSinkMap(int SizeHint = 0) :
		ToPtr(SizeHint, false, 0, NULL),
		ToHnd(SizeHint, false, NULL, 0)
	{
	}

	virtual ~GEventSinkMap()
	{
	}

	int AddSink(GEventSinkI *s)
	{
		if (!s || !Lock(_FL))
			return ToPtr.GetNullKey();

		// Find free handle...
		int Hnd;
		while (ToPtr.Find(Hnd = LgiRand(10000) + 1))
			;

		// Add the new sink
		ToPtr.Add(Hnd, s);
		ToHnd.Add(s, Hnd);

		Unlock();

		return Hnd;
	}

	bool RemoveSink(GEventSinkI *s)
	{
		if (!s || !Lock(_FL))
			return false;

		bool Status = false;
		int Hnd = ToHnd.Find(s);
		if (Hnd > 0)
		{
			Status |= ToHnd.Delete(s);
			Status &= ToPtr.Delete(Hnd);
		}
		else
			LgiAssert(!"Not a member of this sink.");

		Unlock();
		return Status;
	}

	bool RemoveSink(int Hnd)
	{
		if (!Hnd || !Lock(_FL))
			return false;

		bool Status = false;
		void *Ptr = ToPtr.Find(Hnd);
		if (Ptr)
		{
			Status |= ToHnd.Delete(Ptr);
			Status &= ToPtr.Delete(Hnd);
		}
		else
			LgiAssert(!"Not a member of this sink.");

		Unlock();
		return Status;
	}

	bool PostEvent(int Hnd, int Cmd, GMessage::Param a = 0, GMessage::Param b = 0)
	{
		if (!Hnd)
			return false;
		if (!Lock(_FL))
			return false;

		GEventSinkI *s = (GEventSinkI*)ToPtr.Find(Hnd);
		bool Status = false;
		if (s)
			Status = s->PostEvent(Cmd, a, b);
		else
			LgiAssert(!"Not a member of this sink.");

		Unlock();
		return false;
	}
};

class LgiClass GMappedEventSink : public GEventSinkI
{
protected:
	int Handle;
	GEventSinkMap *Map;

public:
	GMappedEventSink()
	{
		Map = NULL;
		Handle = 0;
		SetMap(&GEventSinkMap::Dispatch);
	}

	virtual ~GMappedEventSink()
	{
		SetMap(NULL);
	}

	bool SetMap(GEventSinkMap *m)
	{
		if (Map)
		{
			if (!Map->RemoveSink(this))
				return false;
			Map = 0;
			Handle = 0;
		}
		Map = m;
		if (Map)
		{
			Handle = Map->AddSink(this);
			return Handle > 0;
		}
		return true;
	}

	int GetHandle()
	{
		return Handle;
	}
};

/// This class is a worker thread that accepts messages on it's GEventSinkI interface.
/// To use, sub class and implement the OnEvent handler.
class LgiClass GEventTargetThread :
	public GThread,
	public GMutex,
	public GMappedEventSink,
	public GEventTargetI // Sub-class has to implement OnEvent
{
	GArray<GMessage*> Msgs;
	GThreadEvent Event;
	bool Loop;

	// This makes the event name unique on windows to 
	// prevent multiple instances clashing.
	GString ProcessName(GString obj, const char *desc)
	{
		OsProcessId process = LgiGetCurrentProcess();
		OsThreadId thread = LgiGetCurrentThread();
		GString s;
		s.Printf("%s.%s.%i.%i", obj.Get(), desc, process, thread);
		return s;
	}

public:
	GEventTargetThread(GString Name) :
		GThread(Name + ".Thread"),
		GMutex(Name + ".Mutex"),
		Event(ProcessName(Name, "Event"))
	{
		Loop = true;

		Run();
	}
	
	virtual ~GEventTargetThread()
	{
		EndThread();
	}

	void EndThread()
	{
		if (Loop)
		{
			// We can't be locked here, because GEventTargetThread::Main needs
			// to lock to check for messages...
			Loop = false;
			Event.Signal();
			while (!IsExited())
				LgiSleep(10);
		}
	}

	uint32 GetQueueSize()
	{
		return Msgs.Length();
	}

	bool PostEvent(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0)
	{
		if (!Loop)
			return false;
		if (!Lock(_FL))
			return false;
		
		Msgs.Add(new GMessage(Cmd, a, b));
		Unlock();
		
		return Event.Signal();
	}
	
	int Main()
	{
		while (Loop)
		{
			GThreadEvent::WaitStatus s = Event.Wait();
			if (s == GThreadEvent::WaitSignaled)
			{
				while (true)
				{
					GAutoPtr<GMessage> Msg;
					if (Lock(_FL))
					{
						if (Msgs.Length())
						{
							Msg.Reset(Msgs.First());
							Msgs.DeleteAt(0, true);
						}
						Unlock();
					}

					if (!Loop || !Msg)
						break;

					OnEvent(Msg);
				}
			}
			else
			{
				LgiTrace("%s:%i - Event.Wait failed.\n", _FL);
				break;
			}
		}
		
		return 0;
	}
};



#endif