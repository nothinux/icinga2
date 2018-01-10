provider icinga {
	probe object__ctor(void *obj);
	probe object__dtor(void *obj);

	probe ptr__addref(void *ptr);
	probe ptr__release(void *ptr);

	probe wq__full(void *wq);
	probe wq__starved(void *wq);
	probe wq__task__interleaved(void *wq);
	probe wq__task__begin(void *wq);
	probe wq__task__end(void *wq);
};
