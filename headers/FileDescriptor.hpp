/*
  Adpats posix file descriptors to a more c++ like api
  This implementation does not declare copy constructors to avoid stale file descriptors and unwanted destruction on copies
*/
#ifndef FILE_DESCRIPTOR_H_
#define FILE_DESCRIPTOR_H_

#include <vector>
#include <string>
#include <poll.h>

struct FileDescriptor
{
  FileDescriptor(FileDescriptor &&other);
  FileDescriptor(int sockfd);
  FileDescriptor();
  ~FileDescriptor();
  int file_descriptor;
  bool keep_open_on_destructor;

  pollfd poll(int poll_events = POLLIN, int timeout = 0);
  static std::vector<pollfd> poll(std::vector<FileDescriptor *> &sockets, int poll_flag, int timeout);

  constexpr FileDescriptor &operator=(FileDescriptor &&other);
  FileDescriptor &operator=(const FileDescriptor &) = delete;
  bool operator>(const FileDescriptor &other) const;
  bool operator<(const FileDescriptor &other) const;
  bool operator==(const FileDescriptor &other) const;
  bool operator!=(const FileDescriptor &other) const;
};

#endif // FILE_DESCRIPTOR_H_
#ifdef FILE_DESCRIPTOR_IMPLEMENTATION

FileDescriptor::FileDescriptor() : file_descriptor(-1), keep_open_on_destructor(false) {}
FileDescriptor::FileDescriptor(int file_descriptor) : file_descriptor(file_descriptor), keep_open_on_destructor(false) {}
FileDescriptor::~FileDescriptor()
{
  if (keep_open_on_destructor)
  {
    return;
  }
  if (file_descriptor != -1)
  {
    //LOGF("Closing file descriptor %d", file_descriptor);
    //::close(file_descriptor);
  }
}

pollfd FileDescriptor::poll(int events, int timeout)
{
  pollfd fd;
  fd.fd = file_descriptor;
  fd.events = events;
  ::poll(&fd, 1, timeout);
  return fd;
}

std::vector<pollfd> FileDescriptor::poll(std::vector<FileDescriptor *> &file_descriptors, int poll_events, int timeout)
{
  nfds_t size = file_descriptors.size();
  pollfd fds[size];
  for (nfds_t i = 0; i < size; i++)
  {
    FileDescriptor *fd = file_descriptors[i];
    if (fd)
    {
      fds[i].fd = fd->file_descriptor;
      fds[i].events = poll_events;
    }
  }

  int num_events = ::poll((pollfd *)fds, size, timeout);
  if (num_events == 0)
  {
    return std::vector<pollfd>{};
  }

  std::vector<pollfd> poll_result = std::vector<pollfd>();
  for (nfds_t i = 0; i < size; i++)
  {
    if (fds[i].revents & poll_events)
    {
      poll_result.push_back(fds[i]);
    }
  }

  return poll_result;
}

bool FileDescriptor::operator>(const FileDescriptor &other) const
{
  return file_descriptor > other.file_descriptor;
}

bool FileDescriptor::operator<(const FileDescriptor &other) const
{
  return file_descriptor < other.file_descriptor;
}

bool FileDescriptor::operator==(const FileDescriptor &other) const
{
  return file_descriptor == other.file_descriptor;
}

bool FileDescriptor::operator!=(const FileDescriptor &other) const
{
  return file_descriptor != other.file_descriptor;
}

#endif // FILE_DESCRIPTOR_IMPLEMENTATION