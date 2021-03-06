/*LICENSE_START*/
/*
 *  Copyright (C) 2014  Washington University School of Medicine
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*LICENSE_END*/

#include "CaretAssert.h"
#include "EventSpecFileReadDataFiles.h"

using namespace caret;

/**
 * Constructor.
 * 
 * @param loadIntoBrain
 *    Brain into which file is loaded.
 * @param specFile
 *    Spec file that has its files read.
 */
EventSpecFileReadDataFiles::EventSpecFileReadDataFiles(Brain* loadIntoBrain,
                                     SpecFile* specFile)
: Event(EventTypeEnum::EVENT_SPEC_FILE_READ_DATA_FILES)
{
    this->loadIntoBrain = loadIntoBrain;
    this->specFile  = specFile;
    
    this->username = "";
    this->password = "";
    
    CaretAssert(this->loadIntoBrain);
    CaretAssert(this->specFile);
}

/**
 * Destructor.
 */
EventSpecFileReadDataFiles::~EventSpecFileReadDataFiles()
{
    
}

/**
 * @return The spec file that is to have its data files loaded.
 */
SpecFile* 
EventSpecFileReadDataFiles::getSpecFile()
{
    return this->specFile;
}

/**
 * @return The brain into which files is loaded.
 */
Brain* 
EventSpecFileReadDataFiles::getLoadIntoBrain()
{
    return this->loadIntoBrain;
}
                                     
/**
 * @return The username.
 */
AString 
EventSpecFileReadDataFiles::getUsername() const
{
    return this->username;
}

/**
 * @return The password.
 */
AString 
EventSpecFileReadDataFiles::getPassword() const
{
    return this->password;
}

/**
 * Set the username and password.
 *
 * @param username
 *     Name of user account.
 * @param password
 *     Password of user account.
 */
void 
EventSpecFileReadDataFiles::setUsernameAndPassword(const AString& username,
                                          const AString& password)
{
    this->username = username;
    this->password = password;
}



