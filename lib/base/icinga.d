provider icinga {
	probe object__ctor(void *obj);
	probe object__dtor(void *obj, int had_mutex);

	probe ptr__add_ref(void *ptr);
	probe ptr__release(void *ptr);

	probe wq__full(void *wq);
	probe wq__starved(void *wq);
	probe wq__task__interleaved(void *wq);
	probe wq__task__begin(void *wq);
	probe wq__task__end(void *wq);
};
