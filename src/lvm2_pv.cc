/* Copyright (C) 2012 Mike Fleetwood
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "../include/LVM2_PV_Info.h"
#include "../include/lvm2_pv.h"

namespace GParted
{

FS lvm2_pv::get_filesystem_support()
{
	FS fs ;
	fs .filesystem = GParted::FS_LVM2_PV ;

	LVM2_PV_Info lvm2_pv_info ;
	if ( lvm2_pv_info .is_lvm2_pv_supported() )
	{
		fs .read = GParted::FS::EXTERNAL ;
	}

	return fs ;
}

void lvm2_pv::set_used_sectors( Partition & partition )
{
	LVM2_PV_Info lvm2_pv_info ;
	Byte_Value free_bytes = lvm2_pv_info .get_free_bytes( partition .get_path() ) ;
	if ( free_bytes >= 0 )
		partition .Set_Unused( Utils::round( double(free_bytes) / double(partition .sector_size) ) ) ;

	std::vector<Glib::ustring> error_messages = lvm2_pv_info .get_error_messages( partition .get_path() ) ;
	if ( ! error_messages .empty() )
	{
		for ( unsigned int i = 0 ; i < error_messages .size() ; i ++ )
			partition .messages .push_back( error_messages [ i ] ) ;
	}
}

void lvm2_pv::read_label( Partition & partition )
{
	return ;
}

bool lvm2_pv::write_label( const Partition & partition, OperationDetail & operationdetail )
{
	return true ;
}

void lvm2_pv::read_uuid( Partition & partition )
{
}

bool lvm2_pv::write_uuid( const Partition & partition, OperationDetail & operationdetail )
{
	return true ;
}

bool lvm2_pv::create( const Partition & new_partition, OperationDetail & operationdetail )
{
	return true ;
}

bool lvm2_pv::resize( const Partition & partition_new, OperationDetail & operationdetail, bool fill_partition )
{
	return true ;
}

bool lvm2_pv::move( const Partition & partition_new
                  , const Partition & partition_old
                  , OperationDetail & operationdetail
               )
{
	return true ;
}

bool lvm2_pv::copy( const Glib::ustring & src_part_path
                  , const Glib::ustring & dest_part_path
                  , OperationDetail & operationdetail )
{
	return true ;
}

bool lvm2_pv::check_repair( const Partition & partition, OperationDetail & operationdetail )
{
	return true ;
}

} //GParted