// CH376.h
// Includes for CH376 USB interface
//
int         USB_nodecount = 0 ;           // Number of nodes in SD_nodelist
String      USB_nodelist ;
bool        USB_dr ;                      // Drive ready

#ifndef CH376
  #define setup_CH376()                   // Dummy initialize
  #define check_CH376()                   // Dummy check
  #ifndef CH376
    #define connecttofile()      false    // Dummy connect to file
  #endif
#else
#include <Ch376msc.h>                     // https://www.arduinolibraries.info/libraries/ch376msc
Ch376msc* flashDrive = NULL ;             // Initialized by setup

//**************************************************************************************************
//                                  H A N D L E _ I D 3 _ U S B                                    *
//**************************************************************************************************
// Check file on USB drive for ID3 tags and use them to display some info.                         *
// Extended headers are not parsed.                                                                *
//**************************************************************************************************
void handle_ID3_USB ( String &path )
{
  char*  p ;                                                // Pointer to filename
  struct ID3head_t                                          // First part of ID3 info
  {
    char    fid[3] ;                                        // Should be filled with "ID3"
    uint8_t majV, minV ;                                    // Major and minor version
    uint8_t hflags ;                                        // Headerflags
    uint8_t ttagsize[4] ;                                   // Total tag size
  } ID3head ;
  uint8_t  exthsiz[4] ;                                     // Extended header size
  uint32_t stx ;                                            // Ext header size converted
  uint32_t sttg ;                                           // Total tagsize converted
  uint32_t stg ;                                            // Size of a single tag
  struct ID3tag_t                                           // Tag in ID3 info
  {
    char    tagid[4] ;                                      // Things like "TCON", "TYER", ...
    uint8_t tagsize[4] ;                                    // Size of the tag
    uint8_t tagflags[2] ;                                   // Tag flags
  } ID3tag ;
  uint8_t  tmpbuf[4] ;                                      // Scratch buffer
  uint8_t  tenc ;                                           // Text encoding
  String   albttl = String() ;                              // Album and title
  String   dir ;                                            // Directory from full path
  String   filename ;                                       // filename from full path
  int      inx ;                                            // For splitting full path
  int      res ;
  
  tftset ( 2, "Playing from local file" ) ;                 // Assume no ID3
  inx = path.lastIndexOf ( "/" ) ;                          // Search for the last slash
  dir = path.substring ( 0, inx ) ;                         // Get the directory
  filename = path.substring ( inx+1 ) ;                     // Get the filename
  p = (char*)path.c_str() + 1 ;                             // Point to filename (after the slash)
  showstreamtitle ( p, true ) ;                             // Show the filename as title (middle part)
  flashDrive->resetFileList() ;                             // You never know
  flashDrive->cd ( dir.c_str(), false ) ;                   // Change to this directory
  flashDrive->setFileName ( filename.c_str() ) ;            // Set filename
  flashDrive->openFile() ;                                  // And open the file again (0x14)
  dbgprint ( "OpenFile %s, len is %d",
             path.c_str(),
             flashDrive->getFileSize() ) ;
  if ( filename.endsWith ( "MU3" ) )                        // Is it a playlist?
  {
    return ;                                                // Yes, no ID's, but leave file open
  }
  flashDrive->readRaw ( (uint8_t*)&ID3head,                 // Read first part of ID3 info
                       sizeof(ID3head) ) ;
  if ( strncmp ( ID3head.fid, "ID3", 3 ) == 0 )
  {
    sttg = ssconv ( ID3head.ttagsize ) ;                    // Convert tagsize
    dbgprint ( "Found ID3 info" ) ;
    if ( ID3head.hflags & 0x40 )                            // Extended header?
    {
      stx = ssconv ( exthsiz ) ;                            // Yes, get size of extended header
      while ( stx-- )
      {
        flashDrive->readRaw ( tmpbuf, 1 ) ;                 // Skip next byte of extended header
      }
    }
    while ( sttg > 10 )                                     // Now handle the tags
    {
      sttg -= flashDrive->readRaw ( (uint8_t*)&ID3tag,
                                   sizeof(ID3tag) ) ;       // Read first part of a tag
      if ( ID3tag.tagid[0] == 0 )                           // Reached the end of the list?
      {
        break ;                                             // Yes, quit the loop
      }
      stg = ssconv ( ID3tag.tagsize ) ;                     // Convert size of tag
      if ( ID3tag.tagflags[1] & 0x08 )                      // Compressed?
      {
        sttg -= flashDrive->readRaw ( tmpbuf, 4 ) ;         // Yes, ignore 4 bytes
        stg -= 4 ;                                          // Reduce tag size
      }
      if ( ID3tag.tagflags[1] & 0x044 )                     // Encrypted or grouped?
      {
        sttg -= flashDrive->readRaw ( tmpbuf, 1 ) ;         // Yes, ignore 1 byte
        stg-- ;                                             // Reduce tagsize by 1
      }
      if ( stg > ( sizeof(metalinebf) + 2 ) )               // Room for tag?
      {
        break ;                                             // No, skip this and further tags
      }
      sttg -= flashDrive->readRaw ( (uint8_t*)metalinebf,
                                   stg ) ;                  // Read tag contents
      metalinebf[stg] = '\0' ;                              // Add delimeter
      tenc = metalinebf[0] ;                                // First byte is encoding type
      if ( tenc == '\0' )                                   // Debug all tags with encoding 0
      {
        dbgprint ( "ID3 %s = %s", ID3tag.tagid,
                   metalinebf + 1 ) ;
      }
      if ( ( strncmp ( ID3tag.tagid, "TALB", 4 ) == 0 ) ||  // Album title
           ( strncmp ( ID3tag.tagid, "TPE1", 4 ) == 0 ) )   // or artist?
      {
        albttl += String ( metalinebf + 1 ) ;               // Yes, add to string
        if ( displaytype == T_NEXTION )                     // NEXTION display?
        {
          albttl += String ( "\\r" ) ;                      // Add code for newline (2 characters)
        }
        else
        {
          albttl += String ( "\n" ) ;                       // Add newline (1 character)
        }
      }
      if ( strncmp ( ID3tag.tagid, "TIT2", 4 ) == 0 )       // Songtitle?
      {
        tftset ( 2, metalinebf + 1 ) ;                      // Yes, show title
      }
    }
    tftset ( 1, albttl ) ;                                  // Show album and title
  }
  flashDrive->closeFile() ;                                 // Close the file
  flashDrive->cd ( dir.c_str(), false ) ;                   // Change to this directory
  flashDrive->setFileName ( filename.c_str() ) ;            // Set filename
  flashDrive->openFile() ;                                  // And open the file again
}


//**************************************************************************************************
//                               C O N N E C T T O F I L E _ U S B                                 *
//**************************************************************************************************
// Open the local mp3-file.                                                                        *
//**************************************************************************************************
bool connecttofile_USB()
{
  String path ;                                           // Full file spec
  bool   res ;                                            // For getEOF()

  stop_mp3client() ;                                      // Disconnect if still connected
  tftset ( 0, "ESP32 MP3 Player" ) ;                      // Set screen segment top line
  displaytime ( "" ) ;                                    // Clear time on TFT screen
  datamode = DATA ;                                       // Assume to start in datamode
  if ( host.endsWith ( ".m3u" ) )                         // Is it an m3u playlist?
  {
    playlist = host ;                                     // Save copy of playlist URL
    datamode = PLAYLISTINIT ;                             // Yes, start in PLAYLIST mode
    if ( playlist_num == 0 )                              // First entry to play?
    {
      playlist_num = 1 ;                                  // Yes, set index
    }
    dbgprint ( "Playlist request, entry %d", playlist_num ) ;
  }
  flashDrive->closeFile() ;                               // Sure to close previous file
  path = host.substring ( 9 ) ;                           // Path, skip the "localhost" part
  claimSPI ( "usbopen3" ) ;                               // Claim SPI bus
  handle_ID3_USB ( path ) ;                               // See if there are ID3 tags in this file
  res = flashDrive->getEOF() ;                            // Check EOF
  releaseSPI() ;                                          // Release SPI bus
  if ( res )                                              // Already at EOF?
  {
    dbgprint ( "EOF Error opening file %s",               // No luck
               path.c_str() ) ;
    return false ;
  }
  mp3filelength = flashDrive->getFileSize() ;             // Get length
  mqttpub.trigger ( MQTT_STREAMTITLE ) ;                  // Request publishing to MQTT
  icyname = "" ;                                          // No icy name yet
  chunked = false ;                                       // File not chunked
  metaint = 0 ;                                           // No metadata
  return true ;
}


//**************************************************************************************************
//                                      L I S T U S B T R A C K S                                  *
//**************************************************************************************************
// Search all MP3 files on directory of USB drive.  Return the number of files found.              *
// A "node" of max. 4 levels ( the subdirectory level) will be generated for every file.           *
// The numbers within the node-array is the sequence number of the file/directory in that          *
// subdirectory.                                                                                   *
// A node ID is a string like "2,1,4,0", which means the 4th file in the first sub-directory       *
// of the second directory.                                                                        *
// The list will be send to the webinterface if parameter "send" is true.                          *
// Dirname is like "/DIR1/DIR2/DIR3".                                                              *
//**************************************************************************************************
int listusbtracks ( const char* dirname, int level = 0, bool send = true )
{
  const  uint16_t USB_MAXDEPTH = 4 ;                    // Maximum depts.  Note: see mp3play_html.
  static uint16_t fcount, oldfcount ;                   // Total number of files
  static uint16_t USB_node[USB_MAXDEPTH + 1] ;          // Node filecount, max levels deep
  static String   USB_outbuf ;                          // Output buffer for cmdclient
  String          filename ;                            // Copy of filename for lowercase test
  uint16_t        i ;                                   // Loop control to compute single node id
  String          tmpstr ;                              // Tijdelijke opslag node ID
  const char*     p ;                                   // Points to filename in directory
  bool            found ;                               // Found directory entry
  uint8_t         res ;                                 // Result cd
  uint8_t         attr ;                                // File attribute
  
  dbgprint ( "List dirname %s, level %d", dirname, level ) ;
  if ( strcmp ( dirname, "/" ) == 0 )                   // Are we at the root directory?
  {
    fcount = 0 ;                                        // Yes, reset count
    memset ( USB_node, 0, sizeof(USB_node) ) ;          // And sequence counters
    USB_outbuf = String() ;                             // And output buffer
    USB_nodelist = String() ;                           // And nodelist
    if ( !USB_dr )                                      // See if known card
    {
      if ( send )
      {
        cmdclient.println ( "0/No tracks found" ) ;     // No SD card, emppty list
      }
      return 0 ;
    }
  }
  oldfcount = fcount ;                                  // To see if files found in this directory
  dbgprint ( "USB directory is <%s>", dirname ) ;       // Show current directory
  claimSPI ( "USB_1" ) ;                                // Claim SPI bus
  flashDrive->closeFile() ;
  flashDrive->resetFileList() ;
  res = flashDrive->cd ( dirname, false ) ;             // Dive into directory
  releaseSPI() ;                                        // Release SPI bus
  //if ( !root || !root.isDirectory() )                 // Success?
  //{
  //  dbgprint ( "%s is not a directory or not root",     // No, print debug message
  //             dirname ) ;
  //  return fcount ;                                     // and return
  //}
  while ( true )                                        // Find all (mp3) files
  {
    claimSPI ( "USB_2" ) ;                              // Claim SPI bus
    if ( ( found = flashDrive->listDir() ) )            // Try to open next
    {
      attr = flashDrive->getFileAttrb() ;               // Get file attributes
      filename = flashDrive->getFileName() ;            // File- or subdirectory name
    }
    else
    {
      attr = flashDrive->getFileAttrb() ;               // Get file attributes
      filename = flashDrive->getFileName() ;            // File- or subdirectory name
      dbgprint ( "ListDir fout, attr is 0x%X, filename is %s",
                 attr, filename.c_str() ) ;
      delay ( 1000 ) ;
      found = true ; 
    }
    releaseSPI() ;                                      // Release SPI bus
    if ( !found )
    {
      break ;                                           // End of list
    }
    USB_node[level]++ ;                                 // Set entry sequence of current level
    filename.trim() ;                                   // Remove trailing spaces
    if ( attr == ATTR_DIRECTORY )                       // Is it a directory?
    {
      if ( filename.startsWith ( "." ) )                // Special entry?
      {
        continue ;                                      // Yes, skip it
      }
      dbgprint ( "Directory %s found, attr = 0x%X",
                 filename.c_str(),
                 attr ) ;
      // ATTR_READ_ONLY = 0x01; //read-only file
      // ATTR_HIDDEN = 0x02; //hidden file
      // ATTR_SYSTEM = 0x04; //system file
      // ATTR_VOLUME_ID = 0x08; //Volume label
      // ATTR_DIRECTORY = 0x10; //subdirectory (folder)
      // ATTR_ARCHIVE = 0x20; //archive (normal) file
      if ( level < (USB_MAXDEPTH-1) )                   // Yes, dig deeper
      {
        if ( strlen ( dirname ) > 1 )                   // Are we in subdirectory?
        {
          filename = String ( "/" ) + filename ;        // Yes, insert a slash  
        }
        filename = String ( dirname ) + filename ;
        listusbtracks ( filename.c_str(),               // Note: called recursively
                        level+1, send ) ;
        USB_node[level + 1] = 0 ;                       // Forget counter for one level up
        dbgprint ( "Back to <%s>, level %d",
                   dirname, level ) ;
        claimSPI ( "USB_3" ) ;                          // Claim SPI bus
        res = flashDrive->cd ( dirname, false ) ;       // Back to parent directory
        dbgprint ( "Skip %d entries", USB_node[level] ) ;
        for ( i = USB_node[level] ; i > 0 ; i-- )       // Skip already handled entries
        {
          if ( flashDrive->listDir() )                  // Skip next file entry
          {
            dbgprint ( "Skip <%s>", flashDrive->getFileName() ) ;
          }
        }
        releaseSPI() ;                                  // Release SPI bus
      }
    }
    else
    {
      dbgprint ( "Found file <%s>", filename.c_str() ) ;
      if ( filename.endsWith ( "MP3" ) )                // It is a file, but is it an MP3?
      {
        fcount++ ;                                      // Yes, count total number of MP3 files
        tmpstr = String() ;                             // Empty
        for ( i = 0 ; i < USB_MAXDEPTH ; i++ )          // Add a line containing the node to USB_outbuf
        {
          if ( i )                                      // Need to add separating comma?
          {
            tmpstr += String ( "," ) ;                  // Yes, add comma
          }
          tmpstr += String ( USB_node[i] ) ;            // Add sequence number
        }
        if ( send )                                     // Need to add to string for webinterface?
        {
          USB_outbuf += tmpstr +                        // Form line for mp3play_html page
                        filename +                      // Filename
                        String ( "\n" ) ;
        }
        USB_nodelist += tmpstr + String ( "\n" ) ;      // Add to nodelist
        dbgprint ( "Track: %s",                         // Show debug info
                   flashDrive->getFileName() ) ;
        if ( USB_outbuf.length() > 1000 )               // Buffer full?
        {
          cmdclient.print ( USB_outbuf ) ;              // Yes, send it
          USB_outbuf = String() ;                       // Clear buffer
        }
      }
      else if ( filename.endsWith ( "m3u" ) )           // Is it an .m3u file?
      {
        dbgprint ( "Playlist %s", filename.c_str() ) ;  // Yes, show
      }
    }
    if ( send )
    {
      mp3loop() ;                                       // Keep playing
    }
  }
  if ( fcount != oldfcount )                            // Files in this directory?
  {
    USB_outbuf += String ( "-1/ \n" ) ;                 // Spacing in list
  }
  if ( USB_outbuf.length() )                            // Flush buffer if not empty
  {
    cmdclient.print ( USB_outbuf ) ;                    // Filled, send it
    USB_outbuf = String() ;                             // Continue with empty buffer
  }
  dbgprint ( "Return from listtracks, count is %d", fcount ) ;
  return fcount ;                                       // Return number of MP3s (sofar)
}




//***********************************************************************************************
//                                S E T U P _ C H 3 7 6                                         *
//***********************************************************************************************
// Initialize the CH376 USB interface.                                                         *
//***********************************************************************************************
void setup_CH376()
{
  char *p ;                                                  // Last debug string

  
  if ( ( ini_block.ch376_cs_pin == 0  ) &&                   // CH376 configured?
       ( ini_block.ch376_int_pin = 0 ) )
  {
    dbgprint ( "CH376 pins not configured!" ) ;
    return ;
  }
  dbgprint ( "Start CH376" ) ;
  if ( flashDrive == NULL )
  {
    flashDrive = new Ch376msc ( ini_block.ch376_cs_pin,
                                ini_block.ch376_int_pin ) ;  // Create an instant for CH376
  }
  flashDrive->init() ;                                       // Init the interface
  USB_dr = flashDrive->driveReady() ;                        // Check drive in USB port
  if ( USB_dr )                                              // Report accordingly
  {
    dbgprint ( "Flash drive ready" ) ;
  }
  else
  {
    p = dbgprint ( "Flash drive not ready" ) ;
    tftlog ( p ) ;                                           // Show error on TFT as well
    return ;
  }
  USB_nodecount = listusbtracks ( "/", 0, false ) ;          // Build nodelist
}


//***********************************************************************************************
//                                C H E C K _ C H 3 7 6                                         *
//***********************************************************************************************
// Check if flash drive is inserted/removed.                                                    *
//***********************************************************************************************
void check_CH376()
{
  bool res ;                                        // Flash drive status
   
  if ( flashDrive->checkIntMessage() )              // Interrupt seen?
  {
    res = flashDrive->getDeviceStatus() ;           // Yes, check device status
    if ( res )                                      // Show according to status
    {
      dbgprint ( "Flash drive attached!" ) ;        // Report change
    }
    else
    {
      dbgprint ( "Flash drive detached!" ) ;
    }
  }
}


//**************************************************************************************************
//                                       R E A D _ U S B D R I V E                                 *
//**************************************************************************************************
// Read a block of data from USB drive file.                                                       *
//**************************************************************************************************
int read_USBDRIVE ( uint8_t* buf, uint32_t len )
{
  int numbytes = 0 ;                                   // Number of bytes read total
  int nr ;                                             // Number of bytes read
  int n  ;                                             // Number of bytes to read

  dbgprint ( "read_USBDRIVE-2" ) ;
  while ( ! flashDrive->getEOF() )                     // Do not pass EOF
  {
    if ( len > 255 )                                   // At least 255 bytes to read?
    {
      n = 255 ;
    }
    else
    {
      n = len ;
    }
    dbgprint ( "readraw, %d bytes", n ) ;

    nr = flashDrive->readRaw ( buf, n ) ;              // Try to read
    //if ( nr != n )                                     // Check result
    //{
      dbgprint ( "Read USB error, nr = %d, "
                 "should be %d", nr, n ) ; 
    //}
    numbytes += nr ;                                   // Count total
    len = len - n ;                                    // Reduce rest to read
    if ( len == 0 )                                    // Done?
    {
      break ;                                          // Yes, stop
    }
  }
  return numbytes ;
}


#endif
