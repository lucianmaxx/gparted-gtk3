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

namespace GParted
{

enum PV_ATTRIBUTE
{
	PVATTR_PV_NAME = 0,
	PVATTR_PV_FREE = 1,
	PVATTR_VG_NAME = 2,
	PVATTR_VG_BITS = 3,
	PVATTR_LV_NAME = 4,
	PVATTR_LV_BITS = 5
} ;

//Data model:
//  lvm2_pv_info_cache_initialized
//                      - Has the cache been loaded let?
//  lvm_found           - Is the "lvm" command available?
//  lvm2_pv_cache       - String vector storing attributes of a PV.
//                        Attributes are: pv_name,pv_free,vg_name,
//                        vg_attr,lv_name,lv_attr.  Pv_free is the
//                        number of free bytes, see vgs(8) and lvs(8)
//                        for details of vg_attr and lv_attr
//                        respectively.
//                        E.g.
//                        ["/dev/sda10,2147483648,,r-----,,",
//                         "/dev/sda11,2143289344,GParted-VG1,wz--n-,,",
//                         "/dev/sda12,1619001344,GParted-VG2,wz--n-,lvol0,-wi---",
//                         "/dev/sda12,1619001344,GParted-VG2,wz--n-,,",
//                         "/dev/sda13,830472192,GParted_VG3,wz--n-,lvol0,-wi-a-",
//                         "/dev/sda13,830472192,GParted_VG3,wz--n-,lvol1,-wi-a-",
//                         "/dev/sda13,830472192,GParted_VG3,wz--n-,,",
//                         "/dev/sda14,1828716544,GParted-VG4,wzx-n-,lvol0,-wi---",
//                         "/dev/sda14,1828716544,GParted-VG4,wzx-n-,,"]
//  error_messages      - String vector storing whole cache error
//                        messsages.


//Initialize static data elements
bool LVM2_PV_Info::lvm2_pv_info_cache_initialized = false ;
bool LVM2_PV_Info::lvm_found = false ;
std::vector<Glib::ustring> LVM2_PV_Info::lvm2_pv_cache ;
std::vector<Glib::ustring> LVM2_PV_Info::error_messages ;

LVM2_PV_Info::LVM2_PV_Info()
{
}

LVM2_PV_Info::LVM2_PV_Info( bool do_refresh )
{
	if ( do_refresh )
	{
		set_command_found() ;
		load_lvm2_pv_info_cache() ;
		lvm2_pv_info_cache_initialized = true ;
	}
}

LVM2_PV_Info::~LVM2_PV_Info()
{
}

bool LVM2_PV_Info::is_lvm2_pv_supported()
{
	if ( ! lvm2_pv_info_cache_initialized )
		set_command_found() ;
	return ( lvm_found ) ;
}

Glib::ustring LVM2_PV_Info::get_vg_name( const Glib::ustring & path )
{
	initialize_if_required() ;
	return get_pv_attr_by_path( path, PVATTR_VG_NAME ) ;
}

//Return number of free bytes in the PV, or -1 for error.
Byte_Value LVM2_PV_Info::get_free_bytes( const Glib::ustring & path )
{
	initialize_if_required() ;
	Byte_Value free_bytes = -1 ;
	Glib::ustring fb_str = get_pv_attr_by_path( path, PVATTR_PV_FREE ) ;
	if ( fb_str != "" )
	{
		gchar * suffix ;
		free_bytes = (Byte_Value) g_ascii_strtoll( fb_str .c_str(), & suffix, 10 ) ;
		if ( free_bytes < 0 || ( free_bytes == 0 && suffix == fb_str ) )
			//Negative number or conversion failed
			free_bytes = -1 ;
	}
	return free_bytes ;
}

//Report if any LVs are active in the VG stored in the PV.
bool LVM2_PV_Info::has_active_lvs( const Glib::ustring & path )
{
	initialize_if_required() ;
	Glib::ustring vgname = get_pv_attr_by_path( path, PVATTR_VG_NAME ) ;
	if ( vgname == "" )
		//PV not yet included in any VG
		return false ;

	for ( unsigned int i = 0 ; i < lvm2_pv_cache .size() ; i ++ )
	{
		std::vector<Glib::ustring> pv_attrs ;
		Utils::split( lvm2_pv_cache [i], pv_attrs, "," ) ;
		if ( vgname == get_pv_attr_by_row( i, PVATTR_VG_NAME ) )
		{
			Glib::ustring lv_bits = get_pv_attr_by_row( i, PVATTR_LV_BITS ) ;
			//5th "bit" is active status.  E.g.
			//  "-wi---" inactive, "-wi-a-" active, ...
			//  Treat any non-hyphen character as active.
			if ( lv_bits .length() >= 5 && lv_bits [4] != '-' )
				//LV in VG is active
				return true ;
		}
	}
	return false ;
}

//Report if the VG is exported.
bool LVM2_PV_Info::is_vg_exported( const Glib::ustring & vgname )
{
	initialize_if_required() ;

	for ( unsigned int i = 0 ; i < lvm2_pv_cache .size() ; i ++ )
	{
		std::vector<Glib::ustring> pv_attrs ;
		Utils::split( lvm2_pv_cache [i], pv_attrs, "," ) ;
		if ( vgname == get_pv_attr_by_row( i, PVATTR_VG_NAME ) )
		{
			Glib::ustring vg_bits = get_pv_attr_by_row( i, PVATTR_VG_BITS ) ;
			//3rd "bit" is export status.  E.g.
			//  "wz--n-" imported, "wzx-n-" exported.
			//  Treat any non-hyphen character as exported.
			if ( vg_bits .length() >= 3 && vg_bits [2] != '-' )
				//VG is exported
				return true ;
		}
	}
	return false ;
}

std::vector<Glib::ustring> LVM2_PV_Info::get_error_messages( const Glib::ustring & path )
{
	initialize_if_required() ;
	if ( ! error_messages .empty() )
		//Return whole cache error messages as first choice
		return error_messages ;

	std::vector<Glib::ustring> partition_specific_messages ;
	partition_specific_messages .clear() ;
	Glib::ustring temp ;

	//Check for partition specific message: partial VG
	Glib::ustring vg_bits = get_pv_attr_by_path( path, PVATTR_VG_BITS ) ;
	//4rd "bit" is partial flag.  E.g.
	//  "wz--n-" all PVs exist, "wz-pn-" one or move PVs missing.
	//  Treat any non-hyphen character as damaged.
	if ( vg_bits .length() >= 4 && vg_bits [3] != '-' )
	{
		temp = _("One or more Physical Volumes belonging to the Volume Group is missing.") ;
		temp += "\n" ;
		partition_specific_messages .push_back ( temp ) ;
	}

	return partition_specific_messages ;
}

//Private methods

void LVM2_PV_Info::initialize_if_required()
{
	if ( ! lvm2_pv_info_cache_initialized )
	{
		set_command_found() ;
		load_lvm2_pv_info_cache() ;
		lvm2_pv_info_cache_initialized = true ;
	}
}

void LVM2_PV_Info::set_command_found()
{
	//Set status of command found
	lvm_found = ( ! Glib::find_program_in_path( "lvm" ) .empty() ) ;
}

void LVM2_PV_Info::load_lvm2_pv_info_cache()
{
	Glib::ustring output, error ;

	lvm2_pv_cache .clear() ;
	error_messages .clear() ;
	if ( lvm_found )
	{
		//The OS is expected to fully enable LVM, this scan does
		//  not do the full job.  It is included incase anything
		//  is changed not using lvm commands.
		Utils::execute_command( "lvm vgscan", output, error, true ) ;

		//Load LVM2 PV attribute cache.  Output PV attributes in
		//  PV_ATTRIBUTE order
		Glib::ustring cmd = "lvm pvs --config \"log{command_names=0}\" --nosuffix --noheadings --separator , --units b -o pv_name,pv_free,vg_name,vg_attr,lv_name,lv_attr" ;
		if ( ! Utils::execute_command( cmd, output, error, true ) )
		{
			if ( output != "" )
			{
				Utils::tokenize( output, lvm2_pv_cache, "\n" ) ;
				for ( unsigned int i = 0 ; i < lvm2_pv_cache .size() ; i ++ )
					lvm2_pv_cache [i] = Utils::trim( lvm2_pv_cache [i] ) ;
			}
		}
		else
		{
			error_messages .push_back( cmd ) ;
			if ( ! output .empty() )
				error_messages .push_back ( output ) ;
			if ( ! error .empty() )
				error_messages .push_back ( error ) ;
			Glib::ustring temp ;
			temp = _("An error occurred reading LVM2 configuration!") ;
			temp += "\n" ;
			temp += _("Some or all of the details might be missing or incorrect.") ;
			temp += "\n" ;
			temp += _("You should NOT modify any LVM2 PV partitions.") ;
			temp += "\n" ;
			error_messages .push_back( temp ) ;
		}
	}
}

//Return PV's nth attribute.  Performs linear search of the cache and
//  uses the first matching PV entry.  Attributes are numbered 0 upward
//  using PV_ATTRIBUTE enumeration.
Glib::ustring LVM2_PV_Info::get_pv_attr_by_path( const Glib::ustring & path, unsigned int entry )
{
	for ( unsigned int i = 0 ; i < lvm2_pv_cache .size() ; i ++ )
	{
		std::vector<Glib::ustring> pv_attrs ;
		Utils::split( lvm2_pv_cache [i], pv_attrs, "," ) ;
		if ( PVATTR_PV_NAME < pv_attrs .size() && path == pv_attrs [PVATTR_PV_NAME] )
		{
			if ( entry < pv_attrs .size() )
				return pv_attrs [entry] ;
			else
				break ;
		}
	}

	return "" ;
}

//Return PV's nth attribute from specified cache row.  Row is numbered
//  0 upwards and attributes are numbers numbered 0 upwards using
//  PV_ATTRIBUTE enumeration.
Glib::ustring LVM2_PV_Info::get_pv_attr_by_row( unsigned int row, unsigned int entry )
{
	if ( row >= lvm2_pv_cache .size() )
		return "" ;
	std::vector<Glib::ustring> pv_attrs ;
	Utils::split( lvm2_pv_cache [row], pv_attrs, "," ) ;
	if ( entry < pv_attrs .size() )
		return pv_attrs [entry] ;
	return "" ;
}

}//GParted