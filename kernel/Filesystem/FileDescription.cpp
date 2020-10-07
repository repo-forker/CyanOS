#include "FileDescription.h"
#include "FSNode.h"
#include "FileSystemDefinitions.h"
#include "VirtualFilesystem.h"
#include <Clib.h>
#include <ErrorCodes.h>

Result<UniquePointer<FileDescription>> FileDescription::open(PathView path, OpenMode mode, OpenFlags flags)
{
	auto node = VFS::get_node(path, mode, flags);
	if (node.error()) {
		return ResultError(node.error());
	}

	auto description = UniquePointer<FileDescription>::make_unique(node.value(), mode);

	auto open_ret = node.value().open(*description);
	if (open_ret.is_error()) {
		return ResultError(open_ret.error());
	}

	return description;
}

FileDescription::FileDescription(FSNode& node, OpenMode mode) : m_node(node), m_type{node.m_type}, m_mode{mode} {}

FileDescription::~FileDescription()
{
	m_node.close(*this);
}

Result<size_t> FileDescription::read(void* buff, size_t size)
{
	size_t reading_size = m_current_position + size;
	if (reading_size > m_node.m_size && m_node.m_size != 0) {
		return ResultError(ERROR_EOF);
	}
	return m_node.read(*this, buff, m_current_position, size);
}

Result<size_t> FileDescription::write(const void* buff, size_t size)
{
	size_t offset = m_current_position + size;
	if (offset > m_node.m_size && m_node.m_size != 0) {
		return ResultError(ERROR_EOF);
	}
	return m_node.write(*this, buff, offset, size);
}

Result<UniquePointer<FileDescription>> FileDescription::connect()
{
	auto connection_desc = m_node.connect();
	if (connection_desc.is_error())
		ResultError(connection_desc.error());

	return UniquePointer<FileDescription>::make_unique(connection_desc.value(), OpenMode::OM_CLIENT);
}

Result<UniquePointer<FileDescription>> FileDescription::accept()
{
	auto connection_desc = m_node.accept();
	if (connection_desc.is_error())
		ResultError(connection_desc.error());

	return UniquePointer<FileDescription>::make_unique(connection_desc.value(), OpenMode::OM_SERVER);
}

Result<void> FileDescription::seek(int offset, SeekOrigin origin)
{
	switch (origin) {
		case SeekOrigin::SET: {
			if (offset < 0)
				return ResultError(ERROR_EOF);
			if (size_t(offset) > m_node.m_size)
				return ResultError(ERROR_EOF);

			m_current_position = offset;
			break;
		}
		case SeekOrigin::CURRENT: {
			if (((offset > 0) && (size_t(offset) > (m_node.m_size - m_current_position))) ||
			    ((offset < 0) && (size_t(-offset) > m_current_position)))
				return ResultError(ERROR_EOF);

			m_current_position = offset + m_current_position;
			break;
		}
		case SeekOrigin::END: {
			if (offset > 0)
				return ResultError(ERROR_EOF);
			if (size_t(-offset) > m_node.m_size)
				return ResultError(ERROR_EOF);

			m_current_position = m_node.m_size + offset;
			break;
		}
		default:
			break;
	}
	return ResultError(ERROR_SUCCESS);
}

Result<void> FileDescription::dir_query(DirectoryInfo* info)
{
	auto file_name = m_node.dir_query(m_current_position);
	if (file_name.is_error())
		return ResultError(file_name.error());

	memcpy(info->file_name, file_name.value().data(), file_name.value().length());
	info->file_name[file_name.value().length()] = 0;
	m_current_position++;

	return ResultError(ERROR_SUCCESS);
}

Result<FileInfo> FileDescription::fstat()
{
	return FileInfo{m_node.m_size};
}

Result<void> FileDescription::ioctl()
{
	return ResultError(ERROR_INVALID_PARAMETERS);
}

Result<void> FileDescription::mmap()
{
	return ResultError(ERROR_INVALID_PARAMETERS);
}