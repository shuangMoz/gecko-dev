/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsContainerFrame.h"
#include "nsReflowPath.h"
#include "nsIStyleContext.h"
#include "nsIViewManager.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsHTMLParts.h"
#include "nsIPresContext.h"
#include "nsIFrameManager.h"

nsresult
nsAbsoluteContainingBlock::FirstChild(const nsIFrame* aDelegatingFrame,
                                      nsIAtom*        aListName,
                                      nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsLayoutAtoms::absoluteList == aListName, "unexpected child list name");
  *aFirstChild = mAbsoluteFrames.FirstChild();
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::SetInitialChildList(nsIFrame*       aDelegatingFrame,
                                               nsIPresContext* aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  NS_PRECONDITION(nsLayoutAtoms::absoluteList == aListName, "unexpected child list name");
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  mAbsoluteFrames.SetFrames(aChildList);
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::AppendFrames(nsIFrame*       aDelegatingFrame,
                                        nsIPresContext* aPresContext,
                                        nsIPresShell&   aPresShell,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;

  // Append the frames to our list of absolutely positioned frames
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.AppendFrames(nsnull, aFrameList);

  // Generate a reflow command to reflow the dirty frames
  nsHTMLReflowCommand* reflowCmd;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, eReflowType_ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
    aPresShell.AppendReflowCommand(reflowCmd);
  }

  return rv;
}

nsresult
nsAbsoluteContainingBlock::InsertFrames(nsIFrame*       aDelegatingFrame,
                                        nsIPresContext* aPresContext,
                                        nsIPresShell&   aPresShell,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aPrevFrame,
                                        nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;

  // Insert the new frames
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  // Generate a reflow command to reflow the dirty frames
  nsHTMLReflowCommand* reflowCmd;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, eReflowType_ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
    aPresShell.AppendReflowCommand(reflowCmd);
  }

  return rv;
}

nsresult
nsAbsoluteContainingBlock::RemoveFrame(nsIFrame*       aDelegatingFrame,
                                       nsIPresContext* aPresContext,
                                       nsIPresShell&   aPresShell,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aOldFrame)
{
  PRBool result = mAbsoluteFrames.DestroyFrame(aPresContext, aOldFrame);
  NS_ASSERTION(result, "didn't find frame to delete");
  // Because positioned frames aren't part of a flow, there's no additional
  // work to do, e.g. reflowing sibling frames. And because positioned frames
  // have a view, we don't need to repaint
  return result ? NS_OK : NS_ERROR_FAILURE;
}

// Destructor function for the collapse offset frame property
static void
DestroyRectFunc(nsIPresContext* aPresContext,
                nsIFrame*       aFrame,
                nsIAtom*        aPropertyName,
                void*           aPropertyValue)
{
  delete (nsRect*)aPropertyValue;
}

static nsRect*
GetOverflowAreaProperty(nsIPresContext* aPresContext,
                        nsIFrame*       aFrame,
                        PRBool          aCreateIfNecessary = PR_FALSE)
{
  nsCOMPtr<nsIPresShell>     presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      void* value;
  
      frameManager->GetFrameProperty(aFrame, nsLayoutAtoms::overflowAreaProperty,
                                     0, &value);
      if (value) {
        return (nsRect*)value;  // the property already exists

      } else if (aCreateIfNecessary) {
        // The property isn't set yet, so allocate a new rect, set the property,
        // and return the newly allocated rect
        nsRect*  overflow = new nsRect(0, 0, 0, 0);

        frameManager->SetFrameProperty(aFrame, nsLayoutAtoms::overflowAreaProperty,
                                       overflow, DestroyRectFunc);
        return overflow;
      }
    }
  }

  return nsnull;
}

nsresult
nsAbsoluteContainingBlock::Reflow(nsIFrame*                aDelegatingFrame,
                                  nsIPresContext*          aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nscoord                  aContainingBlockWidth,
                                  nscoord                  aContainingBlockHeight,
                                  nsRect&                  aChildBounds)
{
  // Initialize OUT parameter
  aChildBounds.SetRect(0, 0, 0, 0);

  // Make a copy of the reflow state.  If the reason is
  // eReflowReason_Incremental (which should mean either that the target
  // is the frame for which this is the absolute container or that the
  // container changed size due to incremental reflow of its children),
  // then change it to eReflowReason_Resize.
  nsHTMLReflowState reflowState(aReflowState);
  if (eReflowReason_Incremental == reflowState.reason) {
    reflowState.reason = eReflowReason_Resize;
  }

  nsIFrame* kidFrame;
  for (kidFrame = mAbsoluteFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    nsReflowReason  reason = reflowState.reason;

    nsFrameState kidState;
    kidFrame->GetFrameState(&kidState);
    if (NS_FRAME_FIRST_REFLOW & kidState) {
      // The frame has never had a reflow, so change the reason to eReflowReason_Initial
      reason = eReflowReason_Initial;

    } else if (NS_FRAME_IS_DIRTY & kidState) {
      // The frame is dirty so give it the correct reflow reason
      reason = eReflowReason_Dirty;
    }

    // Reflow the frame
    nsReflowStatus  kidStatus;
    ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, reflowState, aContainingBlockWidth,
                        aContainingBlockHeight, kidFrame, reason, kidStatus);

    // Add in the child's bounds
    nsRect  kidBounds;
    kidFrame->GetRect(kidBounds);
    aChildBounds.UnionRect(aChildBounds, kidBounds);

    // If the frame has visible overflow, then take it into account, too.
    nsFrameState  kidFrameState;
    kidFrame->GetFrameState(&kidFrameState);
    if (kidFrameState & NS_FRAME_OUTSIDE_CHILDREN) {
      // Get the property
      nsRect* overflowArea = ::GetOverflowAreaProperty(aPresContext, kidFrame);

      if (overflowArea) {
        // The overflow area is in the child's coordinate space, so translate
        // it into the parent's coordinate space
        nsRect  rect(*overflowArea);

        rect.MoveBy(kidBounds.x, kidBounds.y);
        aChildBounds.UnionRect(aChildBounds, rect);
      }
    }
  }
  return NS_OK;
}

void
nsAbsoluteContainingBlock::CalculateChildBounds(nsIPresContext* aPresContext,
                                                nsRect&         aChildBounds)
{
  for (nsIFrame* f = mAbsoluteFrames.FirstChild(); f; f->GetNextSibling(&f)) {
    // Add in the child's bounds
    nsRect  bounds;
    f->GetRect(bounds);
    aChildBounds.UnionRect(aChildBounds, bounds);
  
    // If the frame has visible overflow, then take it into account, too.
    nsFrameState  frameState;
    f->GetFrameState(&frameState);
    if (frameState & NS_FRAME_OUTSIDE_CHILDREN) {
      // Get the property
      nsRect* overflowArea = ::GetOverflowAreaProperty(aPresContext, f);
  
      if (overflowArea) {
        // The overflow area is in the child's coordinate space, so translate
        // it into the parent's coordinate space
        nsRect  rect(*overflowArea);
  
        rect.MoveBy(bounds.x, bounds.y);
        aChildBounds.UnionRect(aChildBounds, rect);
      }
    }
  }
}

nsresult
nsAbsoluteContainingBlock::IncrementalReflow(nsIFrame*                aDelegatingFrame,
                                             nsIPresContext*          aPresContext,
                                             const nsHTMLReflowState& aReflowState,
                                             nscoord                  aContainingBlockWidth,
                                             nscoord                  aContainingBlockHeight,
                                             PRBool&                  aWasHandled,
                                             nsRect&                  aChildBounds)
{
  // Initialize the OUT paremeters
  aWasHandled = PR_FALSE;
  aChildBounds.SetRect(0, 0, 0, 0);

  // See if the reflow command is targeted at us.
  nsReflowPath *path = aReflowState.path;
  nsHTMLReflowCommand *command = path->mReflowCommand;

  if (command) {
    // It's targeted at us. See if it's for the positioned child frames
    nsCOMPtr<nsIAtom> listName;
    command->GetChildListName(*getter_AddRefs(listName));

    if (nsLayoutAtoms::absoluteList == listName) {
      nsReflowType  type;

      // Get the type of reflow command
      command->GetType(type);

      // The only type of reflow command we expect is that we have dirty
      // child frames to reflow
      NS_ASSERTION(eReflowType_ReflowDirty, "unexpected reflow type");

      // Walk the positioned frames and reflow the dirty frames
      for (nsIFrame* f = mAbsoluteFrames.FirstChild(); f; f->GetNextSibling(&f)) {
        nsFrameState  frameState;

        f->GetFrameState(&frameState);
        if (frameState & NS_FRAME_IS_DIRTY) {
          nsReflowStatus  status;
          nsReflowReason  reason;

          reason = (frameState & NS_FRAME_FIRST_REFLOW)
            ? eReflowReason_Initial
            : eReflowReason_Dirty;

          ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowState,
                              aContainingBlockWidth, aContainingBlockHeight, f,
                              reason, status);
        }
      }

      // Indicate we handled the reflow command.
      aWasHandled = PR_TRUE;
      
      // Calculate the total child bounds.
      CalculateChildBounds(aPresContext, aChildBounds);
    }
  }

  nsReflowPath::iterator iter = path->FirstChild();
  nsReflowPath::iterator end = path->EndChildren();

  if (iter != end && mAbsoluteFrames.NotEmpty()) {
    // One of our children has been targeted for reflow. By default,
    // assume we've handled it.
    aWasHandled = PR_TRUE;

    for ( ; iter != end; ++iter) {
      // See if it's one of our absolutely positioned child frames
      if (mAbsoluteFrames.ContainsFrame(*iter)) {
        // Remove the next frame from the reflow path
        nsReflowStatus kidStatus;
        ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowState,
                            aContainingBlockWidth, aContainingBlockHeight, *iter,
                            aReflowState.reason, kidStatus);

        // We don't need to invalidate anything because the frame
        // should invalidate any area within its frame that needs
        // repainting, and because it has a view if it changes size
        // the view manager will damage the dirty area

        // Calculate the total child bounds
        CalculateChildBounds(aPresContext, aChildBounds);

        // Prune the path so we don't flow the block frame _again_
        // when returning to the caller.
        aReflowState.path->Remove(iter);
      }
      else {
        // At least one of the frames along the reflow path wasn't
        // absolutely positioned, so we'll need to deal with it in
        // normal block reflow.
        aWasHandled = PR_FALSE;
      }
    }
  }

  return NS_OK;
}

void
nsAbsoluteContainingBlock::DestroyFrames(nsIFrame*       aDelegatingFrame,
                                         nsIPresContext* aPresContext)
{
  mAbsoluteFrames.DestroyFrames(aPresContext);
}

/**
 * NOTE: nsViewportFrame::ReflowFixedFrame is rather similar to this
 * function, so changes made here may also need to be made there.
 */
// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...
nsresult
nsAbsoluteContainingBlock::ReflowAbsoluteFrame(nsIFrame*                aDelegatingFrame,
                                               nsIPresContext*          aPresContext,
                                               const nsHTMLReflowState& aReflowState,
                                               nscoord                  aContainingBlockWidth,
                                               nscoord                  aContainingBlockHeight,
                                               nsIFrame*                aKidFrame,
                                               nsReflowReason           aReason,
                                               nsReflowStatus&          aStatus)
{
  nsresult  rv;
  nsMargin  border;

  // Get the border values
  if (!aReflowState.mStyleBorder->GetBorder(border)) {
    NS_NOTYETIMPLEMENTED("percentage border");
  }
  
  nsFrameState        kidFrameState;
  nsSize              availSize(aReflowState.mComputedWidth, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowMetrics kidDesiredSize(nsnull);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, aKidFrame,
                                     availSize, aContainingBlockWidth,
                                     aContainingBlockHeight,
                                     aReason);

  // Send the WillReflow() notification and position the frame
  aKidFrame->WillReflow(aPresContext);

  // XXXldb We can simplify this if we come up with a better way to
  // position views.
  nscoord x;
  if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left) {
    // Just use the current x-offset
    nsPoint origin;
    aKidFrame->GetOrigin(origin);
    x = origin.x;
  } else {
    x = border.left + kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left;
  }
  aKidFrame->MoveTo(aPresContext, 
                    x, border.top + kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top);

  // Position its view, but don't bother it doing it now if we haven't
  // yet determined the left offset
  if (NS_AUTOOFFSET != kidReflowState.mComputedOffsets.left) {
    nsContainerFrame::PositionFrameView(aPresContext, aKidFrame);
  }

  // Do the reflow
  rv = aKidFrame->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

  // If we're solving for 'left' or 'top', then compute it now that we know the
  // width/height
  if ((NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left) ||
      (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.top)) {
    if (-1 == aContainingBlockWidth) {
      // Get the containing block width/height
      kidReflowState.ComputeContainingBlockRectangle(aPresContext,
                                                     &aReflowState,
                                                     aContainingBlockWidth,
                                                     aContainingBlockHeight);
    }

    if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left) {
      kidReflowState.mComputedOffsets.left = aContainingBlockWidth -
                                             kidReflowState.mComputedOffsets.right -
                                             kidReflowState.mComputedMargin.right -
                                             kidDesiredSize.width -
                                             kidReflowState.mComputedMargin.left;
    }
    if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.top) {
      kidReflowState.mComputedOffsets.top = aContainingBlockHeight -
                                            kidReflowState.mComputedOffsets.bottom -
                                            kidReflowState.mComputedMargin.bottom -
                                            kidDesiredSize.height -
                                            kidReflowState.mComputedMargin.top;
    }
  }
    
  // Position the child relative to our padding edge
  nsRect  rect(border.left + kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left,
               border.top + kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top,
               kidDesiredSize.width, kidDesiredSize.height);
  aKidFrame->SetRect(aPresContext, rect);

  // Size and position the view and set its opacity, visibility, content
  // transparency, and clip
  nsIView*  kidView;
  aKidFrame->GetView(aPresContext, &kidView);
  nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aKidFrame, kidView,
                                             &kidDesiredSize.mOverflowArea);
  aKidFrame->DidReflow(aPresContext, &kidReflowState, NS_FRAME_REFLOW_FINISHED);

  // If the frame has visible overflow, then store it as a property on the
  // frame. This allows us to be able to recover it without having to reflow
  // the frame
  aKidFrame->GetFrameState(&kidFrameState);
  if (kidFrameState & NS_FRAME_OUTSIDE_CHILDREN) {
    // Get the property (creating a rect struct if necessary)
    nsRect* overflowArea = ::GetOverflowAreaProperty(aPresContext, aKidFrame, PR_TRUE);

    if (overflowArea) {
      *overflowArea = kidDesiredSize.mOverflowArea;
    }
  }

  return rv;
}
