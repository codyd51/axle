use crate::desktop::DesktopElement;
use agx_definitions::Rect;
use alloc::collections::{BTreeMap, BTreeSet};
use alloc::rc::Rc;
use alloc::vec;
use alloc::vec::Vec;
use core::cell::RefCell;

pub struct CompositorState {
    desktop_frame: Rect,
    /// While compositing the frame, awm will determine what individual elements
    /// must be redrawn to composite these rectangles.
    /// These may include portions of windows, the desktop background, etc.
    pub rects_to_fully_redraw: RefCell<Vec<Rect>>,
    /// Entire elements that must be composited on the next frame
    pub elements_to_composite: RefCell<BTreeSet<usize>>,
    // Every desktop element the compositor knows about
    pub elements: Vec<Rc<dyn DesktopElement>>,
    pub elements_by_id: BTreeMap<usize, Rc<dyn DesktopElement>>,

    pub extra_draws: RefCell<BTreeMap<usize, BTreeSet<Rect>>>,
    pub extra_background_draws: Vec<Rect>,
}

impl CompositorState {
    pub fn new(desktop_frame: Rect) -> Self {
        Self {
            desktop_frame: desktop_frame,
            rects_to_fully_redraw: RefCell::new(vec![]),
            elements_to_composite: RefCell::new(BTreeSet::new()),
            elements: vec![],
            elements_by_id: BTreeMap::new(),
            extra_draws: RefCell::new(BTreeMap::new()),
            extra_background_draws: vec![],
        }
    }

    pub fn queue_full_redraw(&self, in_rect: Rect) {
        let in_rect = self.desktop_frame.constrain(in_rect);
        if in_rect.is_degenerate() {
            return;
        }
        self.rects_to_fully_redraw.borrow_mut().push(in_rect)
    }

    pub fn queue_composite(&self, element: Rc<dyn DesktopElement>) {
        self.elements_to_composite.borrow_mut().insert(element.id());
    }

    pub fn track_element(&mut self, element: Rc<dyn DesktopElement>) {
        self.elements.push(Rc::clone(&element));
        self.elements_by_id.insert(element.id(), element);
    }

    pub fn queue_extra_draw(&self, element: Rc<dyn DesktopElement>, r: Rect) {
        // Always ensure the extra draw rect is within the bounds of the desktop
        let r = self.desktop_frame.constrain(r);
        if r.is_degenerate() {
            return;
        }
        let element_id = element.id();
        let mut extra_draws = self.extra_draws.borrow_mut();
        if !extra_draws.contains_key(&element_id) {
            extra_draws.insert(element_id, BTreeSet::new());
        }
        let extra_draws_for_element = extra_draws.get_mut(&element_id).unwrap();
        extra_draws_for_element.insert(r);
    }

    pub fn queue_extra_background_draw(&mut self, r: Rect) {
        // Always ensure the extra background draw rect is within the bounds of the desktop
        let r = self.desktop_frame.constrain(r);
        if r.is_degenerate() {
            return;
        }
        self.extra_background_draws.push(r);
    }

    pub fn merge_extra_draws(&self) -> BTreeMap<usize, Vec<Rect>> {
        let mut out = BTreeMap::new();
        for (elem_id, extra_draws) in self.extra_draws.borrow().iter() {
            let mut rects = extra_draws.iter().map(|&r| r).collect::<Vec<Rect>>();

            // Sort by X origin
            rects.sort_by(|a, b| {
                if a.origin.x < b.origin.x {
                    core::cmp::Ordering::Less
                } else if a.origin.x > b.origin.x {
                    core::cmp::Ordering::Greater
                } else {
                    core::cmp::Ordering::Equal
                }
            });

            'begin: loop {
                let mut merged_anything = false;
                //let mut unmerged_rects = rects.clone();

                let rects_clone = rects.clone();
                for (i, r1) in rects_clone.iter().enumerate() {
                    for (_j, r2) in rects_clone[i + 1..].iter().enumerate() {
                        if r1.max_x() == r2.min_x()
                            && r1.min_y() == r2.min_y()
                            && r1.max_y() == r2.max_y()
                        {
                            //println!("Merging {r1} and {r2}");
                            //merged_rects.push(r1.union(*r2));
                            merged_anything = true;
                            // r1 and r2 have been merged
                            rects.retain(|r| r != r1 && r != r2);
                            // TODO(PT): Is the sort order still correct?
                            rects.insert(i, r1.union(*r2));
                            continue 'begin;
                        }
                    }
                }

                // TODO(PT): Are we losing rects?

                if !merged_anything {
                    break;
                }

                //rects = merged_rects.clone();
            }
            out.insert(*elem_id, rects);
        }

        out
    }
}
