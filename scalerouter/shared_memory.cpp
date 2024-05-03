// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "shared_memory.h"

ShmPiece::ShmPiece(std::string name, int size)
{
	this->name = name;
	this->size = size;

	this->shm_fd = -1;
	this->ptr = NULL;
}

ShmPiece::~ShmPiece()
{
	this->remove();
}

bool ShmPiece::open()
{
	/* open shared memory segment */
	this->shm_fd = shm_open(this->name.c_str(), O_CREAT | O_RDWR, 0666);
	
	/* set the size of shared memory segment */
	ftruncate(shm_fd, this->size);
	
	/* now map the shared memory segment in the address space of the process */
	this->ptr = mmap(0, this->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, this->shm_fd, 0);
	
	if (this->ptr == MAP_FAILED){
		LOG_ERROR("Error mapping shared memory " << this->name);
		return false;
	}

	return true;
}

void ShmPiece::remove()
{
	if (this->ptr != MAP_FAILED)
	{
		if (close(this->shm_fd)){
			LOG_ERROR("Error close shm_fd: " << this->shm_fd); 
		}
		else{
			LOG_TRACE("Success close shm_fd: " << this->shm_fd);
		}

		if (munmap(this->ptr, this->size)){
			LOG_ERROR("Error munmap shm_ptr: " << this->ptr);
		}
		else{
			LOG_TRACE("Success munmap shm_ptr: " << this->ptr);
		}

		if (shm_unlink(this->name.c_str()) == -1){
			LOG_ERROR("Error removing shm_name: " << this->name);
		}
		else{
			LOG_TRACE("Success removing shm_name: " << this->name);
		}

		// if(this->ptr){
		// 	// whether free again is required after munmap? no!
		// 	free(this->ptr);
		// 	LOG_TRACE("free shm_ptr");
		// }
	}
}

